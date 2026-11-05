<script lang="ts">
	import { untrack } from 'svelte';
	import * as Card from '$lib/components/ui/card/index.js';
	import { Button } from '$lib/components/ui/button/index.js';
	import UPlotChart, { type ChartTrace } from '$lib/components/UPlotChart.svelte';
	import { usePollTick } from '$lib/amcf/poll.svelte';
	import {
		getDataCaptureInfo,
		getDataCaptureEnvelope,
		getDataCaptureRows,
		getDataCaptureCSV,
		requestSegmentDownloadTicket,
		type DataCaptureInfo,
		type DataCaptureEnvelope,
		type DataCaptureRows
	} from '$lib/amcf/dataCaptureApi';

	// Viewer for a streaming data capture. A capture can hold tens of GB, so the
	// module never loads it as a whole: the chart asks the server for a min/max
	// envelope at the pixel width of the chart, and the table shows a window of rows.

	let { module, app }: { module: any; app: any } = $props();
	const poll = usePollTick();

	const PALETTE = [
		'#6366f1', '#ef4444', '#10b981', '#f59e0b',
		'#3b82f6', '#ec4899', '#14b8a6', '#a855f7'
	];

	// Rows of the table window.
	const PAGE_ROWS = 25;
	// A recording capture grows; its info is refreshed at most this often.
	const INFO_REFRESH_MS = 2000;

	let visible = $derived.by(() => { poll.v; return module.visible !== false; });
	let title = $derived.by(() => { poll.v; return module.title || module.caption || 'Data Capture'; });
	let captureUUID = $derived.by(() => { poll.v; return String(module.captureuuid || '').trim(); });

	let info = $state<DataCaptureInfo | null>(null);
	let selected = $state<string[]>([]);

	// Row window of the chart. It follows the capture until the user zooms.
	let windowFrom = $state(0);
	let windowCount = $state(0);
	let zoomed = $state(false);

	let xValues = $state<number[]>([]);
	let traces = $state<ChartTrace[]>([]);
	let hoveredIdx = $state<number | null>(null);
	let chartWidth = $state(800);

	let tableFrom = $state(0);
	let tableRows = $state<DataCaptureRows | null>(null);

	let errorMsg = $state('');
	let exporting = $state(false);

	// Non-reactive coordination state (must not drive the reactive graph).
	let loadedUUID = '';
	let lastInfoTime = 0;
	let defaultsInitialized = false;
	let lastChartSignature = '';
	let lastTableSignature = '';
	let fetching = false;
	let pending = false;

	let availableEnd = $derived(info ? info.rowCount : 0);
	let availableStart = $derived(info ? info.firstAvailableRow : 0);
	let allColumnNames = $derived(info ? info.columns.map((c) => c.name) : []);

	// The CSV export of the chart window must stay within the response limit of the server.
	let exportValueCount = $derived(windowCount * allColumnNames.length);
	let exportAllowed = $derived(info !== null && exportValueCount > 0 && exportValueCount <= info.maxValueCount);

	function paletteColor (name: string): string {
		const idx = selected.indexOf(name);
		if (idx < 0) return '#94a3b8';
		return PALETTE[idx % PALETTE.length];
	}

	function toggleColumn (name: string) {
		if (selected.includes(name)) {
			selected = selected.filter((n) => n !== name);
		} else {
			if (selected.length >= PALETTE.length) return;
			selected = [...selected, name];
		}
	}

	function handleZoom (minX: number, maxX: number) {
		const from = Math.max(availableStart, Math.floor(minX));
		const to = Math.min(availableEnd, Math.ceil(maxX));
		if (to - from < 2) return;
		windowFrom = from;
		windowCount = to - from;
		zoomed = true;
	}

	function resetZoom () {
		zoomed = false;
		windowFrom = availableStart;
		windowCount = Math.max(0, availableEnd - availableStart);
	}

	function handleHover (idx: number | null) {
		hoveredIdx = idx;
	}

	function clampTableFrom (row: number): number {
		const last = Math.max(availableStart, availableEnd - PAGE_ROWS);
		return Math.min(Math.max(availableStart, Math.floor(row)), last);
	}

	function pageTable (direction: number) {
		tableFrom = clampTableFrom(tableFrom + direction * PAGE_ROWS);
	}

	function showWindowInTable () {
		tableFrom = clampTableFrom(windowFrom);
	}

	// Arrays are only replaced when they hold something: a new empty array is a state change,
	// and `selected` is a dependency of the refresh effect.
	function clearData () {
		info = null;
		if (selected.length > 0) selected = [];
		if (xValues.length > 0) xValues = [];
		if (traces.length > 0) traces = [];
		tableRows = null;
		lastChartSignature = '';
		lastTableSignature = '';
	}

	function applyEnvelope (result: DataCaptureEnvelope) {
		const available = result.series.filter((s) => s.available && s.buckets.length > 0);
		if (available.length === 0) {
			xValues = [];
			traces = [];
			return;
		}

		xValues = available[0].buckets.map((b) => b.row);
		traces = available.map((s) => ({
			label: s.name,
			color: paletteColor(s.name),
			// The line follows the middle of the envelope; the band shows the extremes.
			avg: s.buckets.map((b) => (b.min + b.max) / 2),
			min: s.buckets.map((b) => b.min),
			max: s.buckets.map((b) => b.max)
		}));
	}

	function initializeDefaults (captureInfo: DataCaptureInfo) {
		const names = captureInfo.columns.map((c) => c.name);
		const configured = String(module.defaultcolumns || '').split(',').map((s) => s.trim()).filter((n) => names.includes(n));

		if (configured.length > 0) {
			selected = configured.slice(0, PALETTE.length);
		} else {
			// Without configuration the first two value columns are plotted; a time axis is not a value.
			selected = captureInfo.columns.filter((c) => c.type !== 'timestamp').map((c) => c.name).slice(0, 2);
		}
		defaultsInitialized = true;
	}

	async function doRefresh () {
		if (!app) return;

		const uuid = captureUUID;
		if (!uuid) {
			clearData();
			loadedUUID = '';
			errorMsg = '';
			return;
		}

		try {
			const now = Date.now();
			const isNewCapture = uuid !== loadedUUID;
			const isGrowing = info !== null && info.status === 'recording' && (now - lastInfoTime) > INFO_REFRESH_MS;

			if (isNewCapture || info === null || isGrowing) {
				const captureInfo = await getDataCaptureInfo(app, uuid);
				lastInfoTime = now;

				if (isNewCapture) {
					clearData();
					loadedUUID = uuid;
					defaultsInitialized = false;
					zoomed = false;
					tableFrom = captureInfo.firstAvailableRow;
				}

				info = captureInfo;
				if (!defaultsInitialized)
					initializeDefaults(captureInfo);

				if (!zoomed) {
					windowFrom = captureInfo.firstAvailableRow;
					windowCount = Math.max(0, captureInfo.rowCount - captureInfo.firstAvailableRow);
				}
			}

			const captureInfo = info;
			if (captureInfo === null) return;

			// Chart: one bucket per pixel, whatever the length of the window.
			if (selected.length === 0 || windowCount <= 0) {
				if (xValues.length > 0) xValues = [];
				if (traces.length > 0) traces = [];
				lastChartSignature = '';
			} else {
				const targetPoints = Math.min(Math.max(Math.floor(chartWidth), 100), Math.min(4000, captureInfo.maxTargetPoints || 4000));
				const signature = [uuid, selected.join(','), windowFrom, windowCount, targetPoints].join('|');
				if (signature !== lastChartSignature) {
					applyEnvelope(await getDataCaptureEnvelope(app, uuid, selected, windowFrom, windowCount, targetPoints));
					lastChartSignature = signature;
				}
			}

			// Table: a window of exact rows.
			const names = captureInfo.columns.map((c) => c.name);
			const from = clampTableFrom(tableFrom);
			const count = Math.max(0, Math.min(PAGE_ROWS, captureInfo.rowCount - from));
			if (names.length === 0 || count === 0) {
				tableRows = null;
				lastTableSignature = '';
			} else {
				const signature = [uuid, from, count].join('|');
				if (signature !== lastTableSignature) {
					tableRows = await getDataCaptureRows(app, uuid, names, from, count);
					lastTableSignature = signature;
				}
			}

			errorMsg = '';
		} catch (e: any) {
			errorMsg = app.extractErrorMessage(e);
		}
	}

	async function scheduleRefresh () {
		if (fetching) {
			pending = true;
			return;
		}
		fetching = true;
		try {
			await doRefresh();
		} finally {
			fetching = false;
			if (pending) {
				pending = false;
				scheduleRefresh();
			}
		}
	}

	// React to polling, the configured capture, the column selection and both windows.
	// Data writes happen untracked so they never feed back into this effect.
	$effect(() => {
		poll.v;
		captureUUID;
		selected.join(',');
		windowFrom;
		windowCount;
		tableFrom;
		chartWidth;
		untrack(() => { scheduleRefresh(); });
	});

	async function exportWindowAsCSV () {
		if (!info || !exportAllowed || exporting) return;
		exporting = true;
		try {
			const csv = await getDataCaptureCSV(app, info.uuid, allColumnNames, windowFrom, windowCount);
			const url = URL.createObjectURL(new Blob([csv], { type: 'text/csv' }));
			const anchor = document.createElement('a');
			anchor.href = url;
			anchor.download = `${info.identifier || 'datacapture'}_${windowFrom}_${windowFrom + windowCount - 1}.csv`;
			anchor.click();
			URL.revokeObjectURL(url);
			errorMsg = '';
		} catch (e: any) {
			errorMsg = app.extractErrorMessage(e);
		} finally {
			exporting = false;
		}
	}

	async function downloadSegment (segmentIndex: number) {
		if (!info) return;
		try {
			const ticket = await requestSegmentDownloadTicket(app, info.uuid, segmentIndex);
			if (ticket) app.streamDownload(ticket);
			errorMsg = '';
		} catch (e: any) {
			errorMsg = app.extractErrorMessage(e);
		}
	}

	function trimFraction (text: string): string {
		return text.includes('.') ? text.replace(/0+$/, '').replace(/\.$/, '') : text;
	}

	// The x axis of the chart is the row index. Enough digits are kept to tell the ticks of a
	// zoomed window apart.
	function formatRow (v: number): string {
		if (!isFinite(v)) return '';
		const abs = Math.abs(v);
		if (abs >= 1e9) return trimFraction((v / 1e9).toFixed(6)) + 'G';
		if (abs >= 1e6) return trimFraction((v / 1e6).toFixed(4)) + 'M';
		if (abs >= 1e4) return trimFraction((v / 1e3).toFixed(2)) + 'k';
		return v.toFixed(0);
	}

	function formatNumber (v: number): string {
		if (!isFinite(v)) return '—';
		const abs = Math.abs(v);
		if (abs >= 1000) return v.toFixed(0);
		if (abs >= 1) return v.toFixed(2);
		return v.toFixed(4);
	}

	function formatCell (value: number | boolean | null, type: string): string {
		if (value === null || value === undefined) return '—';
		if (typeof value === 'boolean') return value ? 'true' : 'false';
		if (type === 'float' || type === 'double') return formatNumber(value);
		return String(value);
	}

	function formatBytes (bytes: number): string {
		if (bytes >= 1024 * 1024 * 1024) return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB';
		if (bytes >= 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
		if (bytes >= 1024) return (bytes / 1024).toFixed(1) + ' kB';
		return bytes + ' B';
	}

	function legendValue (trace: ChartTrace): string {
		const idx = hoveredIdx !== null && hoveredIdx < trace.avg.length ? hoveredIdx : trace.avg.length - 1;
		if (idx < 0 || !trace.min || !trace.max) return '—';
		const lo = trace.min[idx];
		const hi = trace.max[idx];
		return lo === hi ? formatNumber(lo) : `${formatNumber(lo)} … ${formatNumber(hi)}`;
	}
</script>

{#if visible}
	<Card.Root class="w-full h-full min-h-0 flex flex-col">
		<Card.Header class="pb-2">
			<div class="flex flex-wrap items-center justify-between gap-2">
				<Card.Title class="text-base">{title}</Card.Title>
				{#if info}
					<div class="flex flex-wrap items-center gap-x-3 gap-y-1 text-xs text-muted-foreground">
						<span>{info.name || info.identifier}</span>
						<span class="uppercase tracking-wide">{info.status}</span>
						<span class="tabular-nums">{info.rowCount.toLocaleString()} rows</span>
						<span class="tabular-nums">{formatBytes(info.storedSize)}</span>
						{#if info.firstAvailableRow > 0}
							<span class="text-amber-600" title="Older rows have been deleted by the storage limit.">
								rows before {info.firstAvailableRow.toLocaleString()} deleted
							</span>
						{/if}
					</div>
				{/if}
			</div>
		</Card.Header>

		<Card.Content class="flex-1 min-h-0 flex flex-col gap-3 overflow-y-auto">
			{#if errorMsg}
				<div class="text-sm text-red-500 border border-red-500/30 rounded p-2 bg-red-500/5">{errorMsg}</div>
			{/if}

			{#if !captureUUID}
				<div class="flex-1 flex items-center justify-center text-sm text-muted-foreground">
					No data capture selected.
				</div>
			{:else if !info}
				<div class="flex-1 flex items-center justify-center text-sm text-muted-foreground">
					Loading data capture…
				</div>
			{:else}
				<!-- Column selection -->
				<div class="flex flex-wrap items-center gap-1.5">
					{#each info.columns as column (column.name)}
						<button
							type="button"
							class="inline-flex items-center gap-1.5 rounded-md border px-2 py-0.5 text-xs hover:bg-muted"
							class:bg-muted={selected.includes(column.name)}
							title={`${column.description} (${column.type})`}
							onclick={() => toggleColumn(column.name)}
						>
							<span class="size-2.5 rounded-sm" style={`background:${paletteColor(column.name)};`}></span>
							<span class="font-medium">{column.name}</span>
						</button>
					{/each}
				</div>

				<!-- Envelope chart over the row window -->
				<div class="border rounded-md p-2 relative min-h-[260px]" bind:clientWidth={chartWidth}>
					{#if selected.length === 0}
						<div class="absolute inset-0 flex items-center justify-center text-sm text-muted-foreground">
							Select one or more columns to plot.
						</div>
					{:else if xValues.length === 0}
						<div class="absolute inset-0 flex items-center justify-center text-sm text-muted-foreground">
							No stored rows in this range.
						</div>
					{:else}
						<UPlotChart
							{xValues}
							{traces}
							height={240}
							formatX={formatRow}
							onZoom={handleZoom}
							onHover={handleHover}
						/>
					{/if}
				</div>

				<div class="flex flex-wrap items-center gap-x-4 gap-y-1 text-xs">
					<span class="text-muted-foreground tabular-nums">
						Rows {windowFrom.toLocaleString()} – {Math.max(windowFrom, windowFrom + windowCount - 1).toLocaleString()}
					</span>
					{#each traces as trace (trace.label)}
						<span class="inline-flex items-center gap-1.5">
							<span class="size-2.5 rounded-sm" style={`background:${trace.color};`}></span>
							<span class="font-medium">{trace.label}</span>
							<span class="tabular-nums text-muted-foreground">{legendValue(trace)}</span>
						</span>
					{/each}
					<span class="flex-1"></span>
					{#if zoomed}
						<Button size="sm" variant="outline" onclick={resetZoom}>Reset zoom</Button>
					{/if}
					<Button size="sm" variant="outline" onclick={showWindowInTable}>Show rows</Button>
					<Button
						size="sm"
						variant="outline"
						disabled={!exportAllowed || exporting}
						title={exportAllowed ? 'Export the rows of the chart window as CSV' : `Zoom in to export: the window holds ${exportValueCount.toLocaleString()} values, the limit is ${info.maxValueCount.toLocaleString()}`}
						onclick={exportWindowAsCSV}
					>
						Export CSV
					</Button>
				</div>

				<!-- Window of exact rows -->
				{#if tableRows && tableRows.columns.length > 0}
					<div class="border rounded-md overflow-x-auto">
						<table class="w-full text-xs tabular-nums">
							<thead class="bg-muted/50">
								<tr>
									<th class="px-2 py-1 text-left font-medium">row</th>
									{#each tableRows.columns as column (column.name)}
										<th class="px-2 py-1 text-right font-medium">{column.name}</th>
									{/each}
								</tr>
							</thead>
							<tbody>
								{#each { length: tableRows.rowCount } as _, rowIndex (rowIndex)}
									<tr class="border-t">
										<td class="px-2 py-0.5 text-muted-foreground">{(tableRows.fromRow + rowIndex).toLocaleString()}</td>
										{#each tableRows.columns as column (column.name)}
											<td class="px-2 py-0.5 text-right">{formatCell(column.values[rowIndex], column.type)}</td>
										{/each}
									</tr>
								{/each}
							</tbody>
						</table>
					</div>
					<div class="flex items-center gap-2 text-xs">
						<Button size="sm" variant="outline" disabled={tableFrom <= availableStart} onclick={() => pageTable(-1)}>Previous</Button>
						<Button size="sm" variant="outline" disabled={tableFrom + PAGE_ROWS >= availableEnd} onclick={() => pageTable(1)}>Next</Button>
					</div>
				{/if}

				<!-- A full capture is exported segment by segment. -->
				{#if info.segments.length > 0}
					<div class="text-xs">
						<div class="font-medium mb-1">Segments</div>
						<div class="max-h-32 overflow-y-auto border rounded-md divide-y">
							{#each info.segments as segment (segment.index)}
								<div class="flex items-center gap-3 px-2 py-1 tabular-nums">
									<span class="w-10 text-muted-foreground">#{segment.index}</span>
									<span class="flex-1">
										rows {segment.firstRow.toLocaleString()} – {Math.max(segment.firstRow, segment.firstRow + segment.rowCount - 1).toLocaleString()}
									</span>
									<span class="text-muted-foreground">{formatBytes(segment.size)}</span>
									{#if segment.available}
										<Button size="sm" variant="ghost" onclick={() => downloadSegment(segment.index)}>Download</Button>
									{:else}
										<span class="text-amber-600">deleted</span>
									{/if}
								</div>
							{/each}
						</div>
					</div>
				{/if}
			{/if}
		</Card.Content>
	</Card.Root>
{/if}
