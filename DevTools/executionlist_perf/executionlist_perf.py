#!/usr/bin/env python3
"""Seed a large build history into a dev amc_server and measure the Build History list.

Before/after tooling for the execution list (N+1 queries per list request). Standard
library only. See README.md next to this file.

Subcommands
  prepare  Build a scratch server directory from a build Output dir (MinimalSystem plus a
           "buildhistory" page with an executionlist), bound to 127.0.0.1, telemetry off.
  seed     Insert N build jobs + executions straight into storage.db (server stopped).
  measure  Fetch the execution list of a running server the way the client does and report
           latency, response size and amc_server CPU.
  run      prepare + start/stop once to migrate the DB + seed + start + measure all modes + stop.

Measure modes (measure --modes, run --modes)
  full        Sequential GET /api/executions: the full list a client fetches once per page load
              (Client/core/modules/AMCModule_ExecutionList.js via AMCListDeltaSync).
  concurrent  --clients full fetches started at once: several tabs/devices loading together.
  delta       Sequential GET /api/executions?since=<head-k> for each k in --delta-sizes: the
              incremental fetch after k executions changed (the client sends one per head change).
  frontend    Sequential GET /api/frontend?pages=buildhistory: the scoped v2 snapshot the
              client polls when it has no SSE stream (the list item only adds its head id).
"""
import argparse, hashlib, json, os, random, re, shutil, signal, socket, sqlite3, subprocess
import sys, threading, time, urllib.error, urllib.parse, urllib.request, uuid
from datetime import datetime, timedelta, timezone

DEFAULT_PORT = 18869
PAGE = "buildhistory"

# --------------------------------------------------------------------------------------------
# prepare

BUILDHISTORY_PAGE = """
		<page name="buildhistory">
			<content name="executionlibrary" title="Build History">
				<executionlist loadingtext="Loading builds" entriesperpage="20" selectevent="" thumbnail="ui_logo">
					<button name="details" caption="Details" event=""/>
				</executionlist>
			</content>
		</page>
"""
BUILDHISTORY_MENU = """			<item id="buildhistory" icon="mdi-history" description="Build History" caption="Build History" targetpage="buildhistory"/>
"""


def prepare(output, workdir, port):
    output, workdir = os.path.abspath(output), os.path.abspath(workdir)
    server_xml = open(os.path.join(output, "amc_server.xml")).read()
    githash = re.search(r'githash="([0-9a-f]+)"', server_xml).group(1)
    os.makedirs(os.path.join(workdir, "data"), exist_ok=True)
    for name in os.listdir(output):
        if not name.startswith(githash + "_") or name.endswith("_config.xml"):
            continue
        link = os.path.join(workdir, name)
        if os.path.lexists(link):
            os.remove(link)
        os.symlink(os.path.join(output, name), link)

    config = open(os.path.join(output, githash + "_config.xml")).read()
    if 'name="buildhistory"' not in config:
        # Insert the page before the menu and a menu entry at the start of the menu.
        config, n = re.subn(r"(\n\s*<menu>\s*\n)", lambda m: "\n" + BUILDHISTORY_PAGE + m.group(1) + BUILDHISTORY_MENU,
                            config, count=1)
        if n != 1:
            sys.exit("prepare: no <menu> in %s_config.xml; is this the MinimalSystem config?" % githash)
    open(os.path.join(workdir, githash + "_config.xml"), "w").write(config)

    server_xml = re.sub(r'<server [^>]*/>', '<server hostname="127.0.0.1" port="%d" />' % port, server_xml)
    server_xml = re.sub(r'<data ([^>]*?)\s*(telemetry="[^"]*")?\s*/>',
                        lambda m: '<data %s telemetry="off" />' % m.group(1), server_xml)
    open(os.path.join(workdir, "amc_server.xml"), "w").write(server_xml)
    return os.path.join(output, "amc_server")


# --------------------------------------------------------------------------------------------
# seed

SEED_NS = uuid.UUID("5eed0000-0000-4000-8000-000000000000")
EMPTY_UUID = "00000000-0000-0000-0000-000000000000"


def seed(db, n, damaged=0):
    """Deterministic and idempotent: removes its own rows (seed-% / seed:%) first.

    Mix: ~85% finished, 10% failed, 5% inprocess (from a fake past journal); every 10th job
    has status 'deleted' (a row, as DeleteJob leaves it); every 3rd job has a
    thumbnailuuid (pointing at no stream, so the browser shows a broken image; the list JSON
    is unaffected).

    incremental_id is set the way bumpIncrementalID would have left it: one step per row, in
    seed order, on top of whatever non-seed rows hold. The list head therefore equals the
    newest seeded execution and ?since=<head-k> returns exactly the last k rows.

    damaged > 0 appends that many executions whose job row does not exist (a damaged
    database). They get the newest incremental ids, so both the full list and every delta
    include them.
    """
    u = lambda kind, i: str(uuid.uuid5(SEED_NS, "%s-%d" % (kind, i)))  # lowercase, hyphenated
    ts = lambda d: d.strftime("%Y-%m-%dT%H:%M:%S.%fZ")                  # convertToISO8601TimeUTC
    rnd = random.Random(42)
    base = datetime(2025, 1, 1, tzinfo=timezone.utc)
    journal = u("journal", 0)
    con = sqlite3.connect(db)
    c = con.cursor()
    c.execute("DELETE FROM buildjobexecutions WHERE description LIKE 'seed:%'")
    c.execute("DELETE FROM buildjobs WHERE name LIKE 'seed-%'")
    job_head = c.execute("SELECT COALESCE(MAX(incremental_id), 0) FROM buildjobs").fetchone()[0]
    exec_head = c.execute("SELECT COALESCE(MAX(incremental_id), 0) FROM buildjobexecutions").fetchone()[0]

    def add_execution(i, exe, job, start, est, end, description):
        c.execute("INSERT INTO buildjobexecutions (uuid,jobuuid,journaluuid,startjournaltimestamp,endjournaltimestamp,useruuid,"
                  "status,description,active,timestamp,incremental_id) VALUES (?,?,?,?,?,?,?,?,?,?,?)",
                  (exe, job, journal, ts(start), ts(end) if est != "inprocess" else 0, EMPTY_UUID, est,
                   description, 1, ts(start), exec_head + i + 1))

    for i in range(n):
        job, exe = u("job", i), u("exec", i)
        start = base + timedelta(minutes=37 * i)
        end = start + timedelta(seconds=rnd.randint(60, 7200))
        jstatus = "deleted" if i % 10 == 9 else "validated"
        thumb = u("thumb", i) if i % 3 == 0 else EMPTY_UUID
        c.execute("INSERT INTO buildjobs (uuid,name,status,storagestreamuuid,layercount,userid,timestamp,useruuid,thumbnailuuid,"
                  "incremental_id) VALUES (?,?,?,?,?,?,?,?,?,?)",
                  (job, "seed-job-%05d" % i, jstatus, u("stream", i), rnd.randint(10, 500), "test", ts(start), EMPTY_UUID, thumb,
                   job_head + i + 1))
        r = rnd.random()
        est = "finished" if r < .85 else ("failed" if r < .95 else "inprocess")
        add_execution(i, exe, job, start, est, end, "seed: execution %d" % i)
    for d in range(damaged):
        start = base + timedelta(minutes=37 * (n + d))
        add_execution(n + d, u("damagedexec", d), u("missingjob", d), start, "finished", start + timedelta(minutes=10),
                      "seed: damaged execution %d (job row missing)" % d)
    con.commit()
    count = c.execute("SELECT count(*) FROM buildjobexecutions WHERE active=1").fetchone()[0]
    head = c.execute("SELECT COALESCE(MAX(incremental_id), 0) FROM buildjobexecutions").fetchone()[0]
    con.close()
    return count, head


# --------------------------------------------------------------------------------------------
# HTTP client

class Api:
    def __init__(self, base, user="test", password="test"):
        self.base = base.rstrip("/")
        H = lambda s: hashlib.sha256(s.encode()).hexdigest()
        s = self._req("/auth/", {"username": user})
        ck = s["sessionkey"]
        salted = H(s["loginsalt"] + password)
        self.token = self._req("/auth/" + s["sessionuuid"],
                               {"clientkey": ck, "password": H(s["sessionkey"] + H(ck + salted))})["token"]

    def _req(self, path, data=None, timeout=600, raw=False):
        headers = {"Content-Type": "application/json"}
        if getattr(self, "token", None):
            headers["Authorization"] = "Bearer " + self.token
        r = urllib.request.Request(self.base + path, headers=headers,
                                   data=json.dumps(data).encode() if data is not None else None)
        with urllib.request.urlopen(r, timeout=timeout) as resp:
            body = resp.read()
        return body if raw else json.loads(body)

    def get(self, path):
        """GET returning (parsed JSON, response bytes). HTTP errors raise ApiError with the server's message."""
        try:
            body = self._req(path, raw=True)
        except urllib.error.HTTPError as e:
            raise ApiError("HTTP %d: %s" % (e.code, e.read()[:300].decode(errors="replace")))
        return json.loads(body), len(body)

    def executions(self, since=None):
        return self.get("/executions" + ("" if since is None else "?since=%d" % since))

    def frontend(self, page):
        return self.get("/frontend?" + urllib.parse.urlencode({"pages": page, "dialogs": ""}))


class ApiError(Exception):
    pass


# --------------------------------------------------------------------------------------------
# CPU sampling from /proc (no sysstat needed)

class CpuSampler(threading.Thread):
    def __init__(self, pid, period=1.0):
        super().__init__(daemon=True)
        self.pid, self.period, self.samples = pid, period, []
        self.tick = os.sysconf("SC_CLK_TCK")
        self._halt = threading.Event()

    def _cpu_seconds(self):
        f = open("/proc/%d/stat" % self.pid).read().rsplit(")", 1)[1].split()
        return (int(f[11]) + int(f[12])) / self.tick  # utime + stime

    def run(self):
        self.t0, self.c0 = time.monotonic(), self._cpu_seconds()
        last_t, last_c = self.t0, self.c0
        while not self._halt.wait(self.period):
            t, c = time.monotonic(), self._cpu_seconds()
            self.samples.append(100.0 * (c - last_c) / (t - last_t))
            last_t, last_c = t, c
        self.t1, self.c1 = time.monotonic(), self._cpu_seconds()

    def stop(self):
        self._halt.set()
        self.join()
        return {"cpu_mean_pct": round(100.0 * (self.c1 - self.c0) / (self.t1 - self.t0), 1),
                "cpu_peak_pct": round(max(self.samples), 1) if self.samples else None}


# --------------------------------------------------------------------------------------------
# measure

def pct(values, p):
    v = sorted(values)
    return v[min(len(v) - 1, int(round(p / 100.0 * (len(v) - 1))))] if v else None


def latency_stats(lat):
    return {"requests": len(lat), "latency_min_s": round(min(lat), 4) if lat else None,
            "latency_median_s": round(pct(lat, 50), 4) if lat else None,
            "latency_max_s": round(max(lat), 4) if lat else None}


def timed(fn, *args):
    """(seconds, result, error) for one request; errors are reported, not raised."""
    t = time.monotonic()
    try:
        res, err = fn(*args), None
    except Exception as e:  # ApiError, timeouts, connection resets
        res, err = None, str(e)
    return time.monotonic() - t, res, err


def sequential(fn, count, *args):
    lat, rows, size, errors = [], None, None, []
    for _ in range(count):
        dt, res, err = timed(fn, *args)
        if err:
            errors.append(err)
            continue
        lat.append(dt)
        data, size = res
        if rows is None and "executions" in data:
            rows = len(data["executions"])
    r = dict(latency_stats(lat), response_bytes=size, errors=len(errors))
    if rows is not None:
        r["rows"] = rows
    if errors:
        r["first_error"] = errors[0]
    return r


def measure_concurrent(api, clients):
    lat, errors, lock = [], [], threading.Lock()

    def one():
        dt, _, err = timed(api.executions)
        with lock:
            (errors.append(err) if err else lat.append(dt))

    t = time.monotonic()
    threads = [threading.Thread(target=one, daemon=True) for _ in range(clients)]
    for th in threads:
        th.start()
    for th in threads:
        th.join()
    r = dict(latency_stats(lat), clients=clients, wall_s=round(time.monotonic() - t, 4), errors=len(errors))
    if errors:
        r["first_error"] = errors[0]
    return r


def measure(base, pid, modes, single_count, clients, delta_sizes, page=PAGE):
    api = Api(base)
    try:
        head = api.executions(0)[0]["executionlistheadid"]  # since=0 is a delta of the whole list; only the head is needed
    except ApiError:
        head = None  # a damaged list fails here too; each mode reports the error itself
    results = {"page": page, "head_id": head, "modes": {}}
    for mode in modes:
        sampler = CpuSampler(pid) if pid else None
        if sampler:
            sampler.start()
        if mode == "full":
            r = sequential(api.executions, single_count)
        elif mode == "concurrent":
            r = measure_concurrent(api, clients)
        elif mode == "delta":
            r = {}
            for k in delta_sizes:
                r["k=%d" % k] = sequential(api.executions, single_count, max(0, (head or 0) - k))
        elif mode == "frontend":
            r = sequential(api.frontend, single_count, page)
        else:
            sys.exit("unknown mode %r" % mode)
        if sampler:
            r["cpu"] = sampler.stop()
        results["modes"][mode] = r
        print("%-10s %s" % (mode, json.dumps(r)), file=sys.stderr, flush=True)
    return results


# --------------------------------------------------------------------------------------------
# run: everything end to end

def wait_port(port, timeout=60):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            socket.create_connection(("127.0.0.1", port), 0.5).close()
            return
        except OSError:
            time.sleep(0.2)
    sys.exit("amc_server did not open port %d within %ds" % (port, timeout))


class Server:
    def __init__(self, binary, workdir, port):
        self.log = open(os.path.join(workdir, "amc_server.log"), "ab")
        self.proc = subprocess.Popen([binary], cwd=workdir, stdout=self.log, stderr=subprocess.STDOUT,
                                     stdin=subprocess.DEVNULL, start_new_session=True)
        wait_port(port)

    def stop(self):
        if self.proc.poll() is None:
            os.killpg(self.proc.pid, signal.SIGTERM)
            try:
                self.proc.wait(10)
            except subprocess.TimeoutExpired:
                os.killpg(self.proc.pid, signal.SIGKILL)
                self.proc.wait()
        self.log.close()
        # Journals are named journal_<YYYYMMDD_HHMMSS>.db; a restart within the same second
        # reuses the file and fails with "table `logs` already exists".
        time.sleep(1.1)


def run(args):
    if args.fresh and os.path.isdir(args.workdir):
        shutil.rmtree(args.workdir)
    binary = prepare(args.output, args.workdir, args.port)
    db = os.path.join(args.workdir, "data", "storage.db")
    if not os.path.exists(db):  # first start creates and migrates the schema
        Server(binary, args.workdir, args.port).stop()
    print("seeded: %d active executions, head id %d" % seed(db, args.n, args.damaged), file=sys.stderr)
    srv = Server(binary, args.workdir, args.port)
    try:
        res = measure("http://127.0.0.1:%d/api" % args.port, srv.proc.pid, args.modes.split(","), args.single_count,
                      args.clients, delta_sizes(args.delta_sizes))
    finally:
        srv.stop()
    res["executions"] = args.n
    res["damaged"] = args.damaged
    res["build_output"] = os.path.abspath(args.output)
    return res


# --------------------------------------------------------------------------------------------

def delta_sizes(text):
    return [int(k) for k in text.split(",") if k.strip()]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    def measure_opts(p):
        p.add_argument("--modes", default="full,concurrent,delta,frontend",
                       help="comma list of full,concurrent,delta,frontend (default: all)")
        p.add_argument("--single-count", type=int, default=5, help="requests per sequential measurement (default 5)")
        p.add_argument("--clients", type=int, default=6, help="simultaneous full fetches in concurrent mode (default 6)")
        p.add_argument("--delta-sizes", default="1,10,100", help="k values for delta mode: rows changed (default 1,10,100)")
        p.add_argument("--json", help="also write the results to this file")

    p = sub.add_parser("prepare", help="create a scratch server dir")
    p.add_argument("--output", required=True, help="build Output dir, e.g. build_linux64/Output")
    p.add_argument("--workdir", required=True)
    p.add_argument("--port", type=int, default=DEFAULT_PORT)

    p = sub.add_parser("seed", help="seed executions into storage.db (stop the server first)")
    p.add_argument("db")
    p.add_argument("-n", type=int, default=2000)
    p.add_argument("--damaged", type=int, default=0, help="extra executions whose job row is missing (default 0)")

    p = sub.add_parser("measure", help="measure against a running server")
    p.add_argument("--base", default="http://127.0.0.1:%d/api" % DEFAULT_PORT)
    p.add_argument("--pid", type=int, help="amc_server pid for CPU sampling")
    p.add_argument("--page", default=PAGE)
    measure_opts(p)

    p = sub.add_parser("run", help="prepare, seed, start, measure, stop")
    p.add_argument("--output", required=True, help="build Output dir, e.g. build_linux64/Output")
    p.add_argument("--workdir", required=True, help="scratch dir for the server (data/ lives here)")
    p.add_argument("--port", type=int, default=DEFAULT_PORT)
    p.add_argument("-n", type=int, default=2000, help="executions to seed (default 2000)")
    p.add_argument("--damaged", type=int, default=0, help="extra executions whose job row is missing (default 0)")
    p.add_argument("--fresh", action="store_true", help="delete --workdir first")
    measure_opts(p)

    args = ap.parse_args()
    if args.cmd == "prepare":
        prepare(args.output, args.workdir, args.port)
        print("prepared %s; start the server from there: (cd %s && %s/amc_server)"
              % (args.workdir, args.workdir, os.path.abspath(args.output)))
        return
    if args.cmd == "seed":
        print("%d active executions, head id %d" % seed(args.db, args.n, args.damaged))
        return
    if args.cmd == "measure":
        res = measure(args.base, args.pid, args.modes.split(","), args.single_count, args.clients,
                      delta_sizes(args.delta_sizes), args.page)
    else:
        res = run(args)
    out = json.dumps(res, indent=2)
    print(out)
    if args.json:
        open(args.json, "w").write(out + "\n")


if __name__ == "__main__":
    main()
