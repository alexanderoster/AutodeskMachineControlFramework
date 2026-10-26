# Execution list performance

`executionlist_perf.py` seeds a large build history into a dev `amc_server` and measures the
Build History list, for before/after comparisons of changes to it. It needs only
Python 3 (standard library) on Linux.

## Usage

```bash
python3 DevTools/executionlist_perf/executionlist_perf.py run \
    --output build_linux64/Output --workdir /tmp/elperf --fresh [-n 2000] [--json result.json]
```

`run` does every step and then stops the server it started:

1. **prepare**: creates `--workdir` from the build's `Output/`. The binaries are symlinked. The
   MinimalSystem config is copied with a `buildhistory` page added (an `executionlist`, as in
   `Examples/LPBFSystem/config.xml`). The server binds to 127.0.0.1:18869 (`--port`) with
   `telemetry="off"`.
2. Starts the server once so that it creates and migrates `data/storage.db`. This step is
   skipped when the database already exists.
3. **seed**: writes N jobs and executions straight into SQLite. The seed is deterministic, and
   rerunning it replaces its own rows. About 85% of executions are finished, 10% failed and 5%
   in process. Every 10th job is `deleted`, and every 3rd job has a thumbnail UUID. Every seeded
   row gets an `incremental_id` (one step per row, above any existing rows), so the list head
   is the newest seeded execution and `?since=<head-k>` returns exactly the last k rows.
4. Starts the server and **measures** each mode in `--modes` (default
   `full,concurrent,delta,frontend`). It logs in as `test`/`test` with the same
   `/api/auth` session-key handshake the client uses, and sends the token as a Bearer header.

The steps can also run one at a time, against a server you start yourself:

```bash
executionlist_perf.py prepare --output build_linux64/Output --workdir W
executionlist_perf.py seed W/data/storage.db -n 2000          # server stopped
executionlist_perf.py measure --pid <amc_server pid> [--modes full,delta --single-count 20]
```

## Modes

The client (`Client/core/modules/AMCModule_ExecutionList.js` with `AMCListDeltaSync`) fetches
the full list once when the page loads, then only a `?since=` delta whenever the
`executionlistheadid` attribute changes. It does not poll the list.

| mode         | what it simulates |
|--------------|-------------------|
| `full`       | Sequential `GET /api/executions` (`--single-count`): one page load. |
| `concurrent` | `--clients` (default 6) full fetches started at once: several tabs or devices loading together. |
| `delta`      | Sequential `GET /api/executions?since=<head-k>` for each k in `--delta-sizes` (default `1,10,100`): the fetch after k executions changed. |
| `frontend`   | Sequential `GET /api/frontend?pages=buildhistory`: the scoped v2 snapshot the client polls when it has no SSE stream. The list item contributes only its head id, so this should not grow with N. |

## Output

- `latency_*_s`: per-request latency.
- `rows`, `response_bytes`: list entries and JSON size of one response.
- `wall_s` (concurrent): time until the last of the simultaneous fetches finished.
- `errors` / `first_error`: failed requests and the server's message for the first.
- `cpu.cpu_mean_pct` / `cpu.cpu_peak_pct`: `amc_server` CPU from `/proc/<pid>/stat`, sampled
  every 1 s over the mode. 100% means one core. Short modes finish before the first sample, so
  only the mean is meaningful there.

Measure an optimised build when you want device-like numbers. An empty `CMAKE_BUILD_TYPE`
compiles at -O0, while device builds use `-DCMAKE_BUILD_TYPE=Release`. Compare before/after
only between builds of the same type.

## Damaged databases

`--damaged K` (on `seed` and `run`) appends K executions whose job row does not exist, as in a
damaged database. They carry the newest `incremental_id`s, so they are in the full list and in
every delta. Before the per-entry degrade fix in LibMCData, one such row makes the whole
`/api/executions` request fail (`errors` > 0), and the client's list stays frozen.

