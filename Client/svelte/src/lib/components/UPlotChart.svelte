<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import uPlot from 'uplot';
	import 'uplot/dist/uPlot.min.css';

	export interface ChartTrace {
		label: string;
		// Human readable name for legends; falls back to `label`.
		title?: string;
		color: string;
		avg: number[];
		min?: number[];
		max?: number[];
	}

	let {
		xValues = [],
		traces = [],
		height = 320,
		fillHeight = false,
		showLiveLabels = false,
		canvasLegend = false,
		onZoom = undefined,
		onHover = undefined
	}: {
		xValues: number[];
		traces: ChartTrace[];
		height?: number;
		// When true the chart follows its container's height instead of the fixed
		// `height` prop (used for space-filling embedded charts).
		fillHeight?: boolean;
		// When true the latest value of each trace is drawn directly on the canvas
		// next to the series, at the right-hand edge of the plot.
		showLiveLabels?: boolean;
		// When true a legend (color, title and the latest or hovered value of each
		// trace) is drawn on the canvas in a band reserved above the plotting area.
		canvasLegend?: boolean;
		onZoom?: (minX: number, maxX: number) => void;
		onHover?: (idx: number | null) => void;
	} = $props();

	let containerEl: HTMLDivElement | undefined = $state(undefined);
	let plot: uPlot | undefined = undefined;
	let lastSignature = '';
	let resizeObserver: ResizeObserver | null = null;

	function hexToRgba (hex: string, alpha: number): string {
		let h = hex.replace('#', '');
		if (h.length === 3) h = h.split('').map((c) => c + c).join('');
		const n = parseInt(h, 16);
		if (isNaN(n)) return `rgba(99,102,241,${alpha})`;
		const r = (n >> 16) & 255;
		const g = (n >> 8) & 255;
		const b = n & 255;
		return `rgba(${r},${g},${b},${alpha})`;
	}

	function seriesSignature (): string {
		return traces.map((t) => `${t.label}:${t.title ?? ''}:${t.color}`).join('|') + `|fill:${fillHeight}|live:${showLiveLabels}|legend:${canvasLegend}`;
	}

	// Resolves the height uPlot should render at. In fillHeight mode the chart tracks
	// its container so it stays space-filling; otherwise it uses the fixed prop.
	function currentHeight (): number {
		if (fillHeight && containerEl) {
			const measured = containerEl.getBoundingClientRect().height;
			if (measured > 0) return Math.floor(measured);
		}
		return height;
	}

	function formatLiveValue (v: number): string {
		if (!isFinite(v)) return '';
		const abs = Math.abs(v);
		if (abs >= 1000) return v.toFixed(0);
		if (abs >= 1) return v.toFixed(2);
		return v.toFixed(4);
	}

	// uPlot draw hook that paints the latest value of every avg-series directly onto
	// the canvas, right-aligned at the plot edge and coloured to match the trace.
	function drawLiveLabels (u: uPlot) {
		const ctx = u.ctx;
		const data = u.data;
		if (!data || data.length < 4) return;

		const rightEdge = u.bbox.left + u.bbox.width;
		const topEdge = u.bbox.top;
		const bottomEdge = u.bbox.top + u.bbox.height;

		ctx.save();
		ctx.font = `${Math.round(11 * (u.ctx.canvas.height / (u.height || 1)))}px system-ui, sans-serif`;
		ctx.textAlign = 'right';
		ctx.textBaseline = 'middle';

		traces.forEach((trace, i) => {
			const avgIdx = 3 + i * 3;
			const arr = data[avgIdx] as (number | null)[] | undefined;
			if (!arr) return;

			// Find the last finite sample of this series.
			let lastVal = NaN;
			for (let k = arr.length - 1; k >= 0; k--) {
				const val = arr[k];
				if (val != null && isFinite(val)) {
					lastVal = val;
					break;
				}
			}
			if (!isFinite(lastVal)) return;

			let cy = u.valToPos(lastVal, 'y', true);
			if (!isFinite(cy)) return;
			// Keep the label inside the plotting area.
			cy = Math.max(topEdge + 8, Math.min(bottomEdge - 8, cy));

			const text = formatLiveValue(lastVal);
			const padX = 6 * (u.ctx.canvas.width / (u.width || 1));
			const metrics = ctx.measureText(text);
			const boxW = metrics.width + padX * 1.6;
			const boxH = 18 * (u.ctx.canvas.height / (u.height || 1));

			// Subtle background chip for readability over grid/bands.
			ctx.fillStyle = 'rgba(255,255,255,0.85)';
			ctx.fillRect(rightEdge - boxW, cy - boxH / 2, boxW, boxH);
			ctx.strokeStyle = trace.color;
			ctx.lineWidth = 1;
			ctx.strokeRect(rightEdge - boxW, cy - boxH / 2, boxW, boxH);

			ctx.fillStyle = trace.color;
			ctx.fillText(text, rightEdge - padX * 0.8, cy);
		});

		ctx.restore();
	}

	// Canvas legend geometry, in CSS pixels.
	const LEGEND_FONT = 11;
	const LEGEND_LINE = 16;
	const LEGEND_PAD = 4;
	const LEGEND_SWATCH = 9;
	const LEGEND_SWATCH_GAP = 5;
	const LEGEND_VALUE_GAP = 6;
	const LEGEND_ITEM_GAP = 16;

	let measureCtx: CanvasRenderingContext2D | null = null;

	function measureText (text: string): number {
		if (!measureCtx) measureCtx = document.createElement('canvas').getContext('2d');
		if (!measureCtx) return text.length * LEGEND_FONT * 0.6;
		measureCtx.font = `${LEGEND_FONT}px system-ui, sans-serif`;
		return measureCtx.measureText(text).width;
	}

	function traceTitle (trace: ChartTrace): string {
		return trace.title || trace.label;
	}

	// Items flow left to right and wrap. The value slot has a fixed width so the
	// layout (and the reserved band height) does not change while hovering.
	function legendLayout (cssWidth: number): { items: { x: number; line: number; titleWidth: number }[]; height: number } {
		const valueWidth = measureText('-0000.00');
		const items: { x: number; line: number; titleWidth: number }[] = [];
		let x = LEGEND_PAD;
		let line = 0;
		for (const trace of traces) {
			const titleWidth = measureText(traceTitle(trace));
			const itemWidth = LEGEND_SWATCH + LEGEND_SWATCH_GAP + titleWidth + LEGEND_VALUE_GAP + valueWidth;
			if (x > LEGEND_PAD && x + itemWidth > cssWidth - LEGEND_PAD) {
				line++;
				x = LEGEND_PAD;
			}
			items.push({ x, line, titleWidth });
			x += itemWidth + LEGEND_ITEM_GAP;
		}
		return { items, height: (line + 1) * LEGEND_LINE + LEGEND_PAD };
	}

	function legendValueAt (arr: (number | null)[] | undefined, idx: number | null | undefined): number {
		if (!arr) return NaN;
		if (idx != null && idx >= 0 && idx < arr.length) {
			const val = arr[idx];
			return (val != null && isFinite(val)) ? val : NaN;
		}
		for (let k = arr.length - 1; k >= 0; k--) {
			const val = arr[k];
			if (val != null && isFinite(val)) return val;
		}
		return NaN;
	}

	function drawLegend (u: uPlot) {
		if (traces.length === 0) return;
		const ctx = u.ctx;
		const ratio = u.ctx.canvas.width / (u.width || 1);
		const layout = legendLayout(u.width);

		ctx.save();
		ctx.font = `${Math.round(LEGEND_FONT * ratio)}px system-ui, sans-serif`;
		ctx.textAlign = 'left';
		ctx.textBaseline = 'middle';

		traces.forEach((trace, i) => {
			const item = layout.items[i];
			const x = item.x * ratio;
			const cy = (LEGEND_PAD + item.line * LEGEND_LINE + LEGEND_LINE / 2) * ratio;

			ctx.fillStyle = trace.color;
			ctx.fillRect(x, cy - (LEGEND_SWATCH / 2) * ratio, LEGEND_SWATCH * ratio, LEGEND_SWATCH * ratio);

			const titleX = x + (LEGEND_SWATCH + LEGEND_SWATCH_GAP) * ratio;
			ctx.fillStyle = '#334155';
			ctx.fillText(traceTitle(trace), titleX, cy);

			const value = legendValueAt(u.data[3 + i * 3] as (number | null)[] | undefined, u.cursor.idx);
			ctx.fillStyle = '#64748b';
			ctx.fillText(formatLiveValue(value) || '—', titleX + (item.titleWidth + LEGEND_VALUE_GAP) * ratio, cy);
		});

		ctx.restore();
	}

	function buildData (): (number[])[] {
		const data: number[][] = [xValues];
		for (const trace of traces) {
			data.push(trace.max ?? trace.avg);
			data.push(trace.min ?? trace.avg);
			data.push(trace.avg);
		}
		return data;
	}

	function buildOptions (width: number): uPlot.Options {
		const series: uPlot.Series[] = [{}];
		const bands: uPlot.Band[] = [];

		traces.forEach((trace, i) => {
			const maxIdx = 1 + i * 3;
			const minIdx = maxIdx + 1;
			series.push({ label: `${trace.label} max`, stroke: 'transparent', points: { show: false } });
			series.push({ label: `${trace.label} min`, stroke: 'transparent', points: { show: false } });
			series.push({
				label: trace.label,
				stroke: trace.color,
				width: 1.5,
				points: { show: false }
			});
			bands.push({ series: [maxIdx, minIdx], fill: hexToRgba(trace.color, 0.15) });
		});

		const drawHooks: ((u: uPlot) => void)[] = [];
		if (showLiveLabels) drawHooks.push(drawLiveLabels);
		if (canvasLegend) drawHooks.push(drawLegend);

		// Repaint the legend only when the hovered sample changes.
		let legendIdx: number | null | undefined = undefined;

		return {
			width: Math.max(width, 100),
			height: currentHeight(),
			legend: { show: false },
			...(canvasLegend ? { padding: [(u: uPlot) => legendLayout(u.width).height + LEGEND_PAD, null, null, null] as uPlot.Padding } : {}),
			cursor: {
				drag: { x: true, y: false, setScale: false },
				focus: { prox: 30 }
			},
			scales: { x: { time: false } },
			axes: [
				{
					stroke: '#94a3b8',
					grid: { stroke: 'rgba(148,163,184,0.15)' },
					ticks: { stroke: 'rgba(148,163,184,0.25)' },
					values: (_u, splits) => splits.map((v) => formatSeconds(v))
				},
				{
					stroke: '#94a3b8',
					grid: { stroke: 'rgba(148,163,184,0.15)' },
					ticks: { stroke: 'rgba(148,163,184,0.25)' }
				}
			],
			series,
			bands,
			hooks: {
				draw: drawHooks,
				setSelect: [
					(u: uPlot) => {
						if (u.select.width <= 2) return;
						const minX = u.posToVal(u.select.left, 'x');
						const maxX = u.posToVal(u.select.left + u.select.width, 'x');
						u.setSelect({ left: 0, top: 0, width: 0, height: 0 }, false);
						if (onZoom && maxX > minX) onZoom(minX, maxX);
					}
				],
				setCursor: [
					(u: uPlot) => {
						if (onHover) onHover(u.cursor.idx ?? null);
						if (canvasLegend && u.cursor.idx !== legendIdx) {
							legendIdx = u.cursor.idx;
							u.redraw(false);
						}
					}
				]
			}
		};
	}

	function formatSeconds (v: number): string {
		if (!isFinite(v)) return '';
		if (v >= 60) {
			const m = Math.floor(v / 60);
			const s = Math.floor(v % 60);
			return `${m}m${s.toString().padStart(2, '0')}s`;
		}
		return `${v.toFixed(v < 10 ? 2 : 0)}s`;
	}

	function rebuild () {
		if (!containerEl) return;
		if (plot) {
			plot.destroy();
			plot = undefined;
		}
		const width = containerEl.getBoundingClientRect().width || 600;
		plot = new uPlot(buildOptions(width), buildData() as uPlot.AlignedData, containerEl);
		lastSignature = seriesSignature();
	}

	function update () {
		if (!containerEl) return;
		const signature = seriesSignature();
		if (!plot || signature !== lastSignature) {
			rebuild();
			return;
		}
		plot.setData(buildData() as uPlot.AlignedData);
	}

	function handleWheel (event: WheelEvent) {
		if (!plot || !onZoom) return;
		event.preventDefault();
		const rect = plot.over.getBoundingClientRect();
		const cursorX = plot.posToVal(event.clientX - rect.left, 'x');
		const minX = plot.scales.x.min ?? xValues[0] ?? 0;
		const maxX = plot.scales.x.max ?? xValues[xValues.length - 1] ?? 1;
		const factor = event.deltaY < 0 ? 0.8 : 1.25;
		const newMin = cursorX - (cursorX - minX) * factor;
		const newMax = cursorX + (maxX - cursorX) * factor;
		if (newMax > newMin) onZoom(newMin, newMax);
	}

	$effect(() => {
		// Track reactive inputs so the chart updates when data changes.
		xValues;
		traces;
		height;
		update();
	});

	onMount(() => {
		rebuild();
		resizeObserver = new ResizeObserver(() => {
			if (plot && containerEl) {
				plot.setSize({
					width: containerEl.getBoundingClientRect().width || 600,
					height: currentHeight()
				});
			}
		});
		if (containerEl) resizeObserver.observe(containerEl);
	});

	onDestroy(() => {
		if (resizeObserver) resizeObserver.disconnect();
		if (plot) {
			plot.destroy();
			plot = undefined;
		}
	});
</script>

<div
	bind:this={containerEl}
	class="w-full"
	style={fillHeight ? 'height: 100%;' : `height: ${height}px;`}
	role="img"
	aria-label="Process parameter history chart"
	onwheel={handleWheel}
></div>
