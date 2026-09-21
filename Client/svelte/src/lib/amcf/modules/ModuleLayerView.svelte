<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import { usePollTick } from '$lib/amcf/poll.svelte';
	import * as Card from '$lib/components/ui/card/index.js';
	import Square from '@lucide/svelte/icons/square';
	import Shapes from '@lucide/svelte/icons/shapes';
	import Axis3d from '@lucide/svelte/icons/axis-3d';
	import Info from '@lucide/svelte/icons/info';
	// @ts-ignore — core JS has no type declarations yet
	import WebGLImpl from '@core/common/AMCImplementation_WebGL.js';
	// @ts-ignore
	import LayerViewImpl from '@core/common/AMCImplementation_LayerView.js';

	const ZOOM_MARGIN = 10;

	let { module, app }: { module: any; app: any } = $props();
	const poll = usePollTick();

	let visible = $derived.by(() => { poll.v; return module.visible !== false; });
	let cardstyle = $derived.by(() => { poll.v; return module.cardstyle || 'none'; });
	let isCard = $derived(cardstyle === 'elevated' || cardstyle === 'outlined' || cardstyle === 'tinted');
	let cardTitle = $derived.by(() => { poll.v; return module.title || ''; });
	let cardSubtitle = $derived.by(() => { poll.v; return module.subtitle || ''; });
	let containerEl: HTMLDivElement | undefined = $state(undefined);
	let glInstance: any = $state(null);
	let layerViewer: any = $state(null);
	let initialized = $state(false);

	type CoordinateTransform = readonly [
		angleDegrees: number,
		rotationCenterX: number,
		rotationCenterY: number,
		translationX: number,
		translationY: number
	];
	let platform = $derived.by(() => { poll.v; return module.platform || null; });
	let layerCount = $derived.by(() => { poll.v; return platform?.layercount || 0; });
	let transformAngle = $derived(Number(platform?.transformangle) || 0);
	let rotationCenterX = $derived(Number(platform?.rotationcenterx) || 0);
	let rotationCenterY = $derived(Number(platform?.rotationcentery) || 0);
	let translationX = $derived(Number(platform?.translationx) || 0);
	let translationY = $derived(Number(platform?.translationy) || 0);
	let coordinateTransform = $derived([
		transformAngle,
		rotationCenterX,
		rotationCenterY,
		translationX,
		translationY
	] as CoordinateTransform);
	let sliderValue = $state(0);
	let appliedColorTheme = $state('');
	let coordinateSystemOverride: boolean | null = $state(null);
	let coordinateSystemVisible = $derived(
		coordinateSystemOverride ?? Boolean(platform?.showcoordinatesystem)
	);
	let appliedCoordinateTransform: CoordinateTransform | null = null;

	$effect(() => {
		poll.v;
		if (platform) {
			sliderValue = platform.currentlayer || 0;
		}
	});

	function getCurrentColorSet(): Record<string, string> | null {
		if (!platform) return null;
		const isDark = document.documentElement.classList.contains('dark');
		return isDark ? platform.darkcolors : platform.colors;
	}

	function getBuildPlateURL(): string | null {
		if (!platform || !app) return null;
		const isDark = document.documentElement.classList.contains('dark');
		if (isDark && platform.dark_baseimageresource) {
			return app.getImageURL(platform.dark_baseimageresource);
		}
		if (platform.baseimageresource) {
			return app.getImageURL(platform.baseimageresource);
		}
		return null;
	}

	$effect(() => {
		poll.v;
		if (!layerViewer || !platform || !initialized) return;
		const isDark = document.documentElement.classList.contains('dark');
		const themeKey = isDark ? 'dark' : 'light';
		if (themeKey !== appliedColorTheme) {
			appliedColorTheme = themeKey;
			const cs = getCurrentColorSet();
			if (cs) layerViewer.applyColors(cs);
			const plateURL = getBuildPlateURL();
			layerViewer.SetBuildPlateSVG(plateURL);
			layerViewer.RenderScene(true);
		}
	});

	$effect(() => {
		poll.v;
		if (!layerViewer || !initialized) return;

		if (coordinateTransform === appliedCoordinateTransform) return;

		layerViewer.setCoordinateTransform(...coordinateTransform);
		appliedCoordinateTransform = coordinateTransform;
		layerViewer.RenderScene(true);
	});

	function ensureInit() {
		if (initialized || !containerEl || !app) return;
		const w = containerEl.clientWidth, h = containerEl.clientHeight;
		if (w === 0 || h === 0) return;

		try {
			glInstance = app.retrieveWebGLInstance(module.uuid);
			if (!glInstance) {
				glInstance = new WebGLImpl();
				app.storeWebGLInstance(module.uuid, glInstance);
			}

			layerViewer = new LayerViewImpl(glInstance);

			glInstance.setupDOMElement(containerEl);
			layerViewer.updateSize(w, h);
			layerViewer.setCoordinateTransform(...coordinateTransform);
			appliedCoordinateTransform = coordinateTransform;

			if (platform) {
				const plateURL = getBuildPlateURL();
				if (plateURL) {
					layerViewer.SetBuildPlateSVG(plateURL);
				}
				layerViewer.setOrigin(platform.originx || 0, platform.originy || 0);
				centerOnPlatform();

				platform.displayed_layer = 0;
				platform.displayed_build = 0;
			}

			const cs = getCurrentColorSet();
			if (cs) {
				layerViewer.applyColors(cs);
				const isDark = document.documentElement.classList.contains('dark');
				appliedColorTheme = isDark ? 'dark' : 'light';
			}

			layerViewer.RenderScene(true);
			initialized = true;
		} catch (e) {
			console.warn('[LayerView] init failed:', e);
		}
	}

	function onDataChanged(sender: any) {
		if (!layerViewer || !platform || !sender) return;
		if (!module.isActive || !module.isActive()) return;
		if (sender.uuid !== module.uuid) return;

		if (platform.displayed_layer !== platform.currentlayer ||
			platform.displayed_build !== platform.builduuid) {

			platform.displayed_layer = platform.currentlayer;
			platform.displayed_build = platform.builduuid;
			sliderValue = platform.currentlayer || 0;

			app.axiosPostRequest('/build/toolpath', {
				builduuid: platform.builduuid,
				layerindex: platform.currentlayer
			})
			.then((layerJSON: any) => {
				if (layerViewer) {
					layerViewer.loadLayer(layerJSON.data.segments);
					layerViewer.RenderScene(true);
				}
			})
			.catch((err: any) => {
				console.warn('[LayerView] layer load error:', err?.response || err);
				if (layerViewer) layerViewer.RenderScene(true);
			});
		}
	}

	function onSliderChange(e: Event) {
		const val = parseInt((e.target as HTMLInputElement).value);
		sliderValue = val;
		if (platform) {
			app.triggerWidgetRequest(platform.uuid, 'changelayer', { targetlayer: val });
		}
	}

	function onWheel(event: WheelEvent) {
		if (!containerEl || !layerViewer) return;
		event.preventDefault();

		let delta = event.deltaY;
		if (delta > 5) delta = 5;
		if (delta < -5) delta = -5;

		const box = containerEl.getBoundingClientRect();
		const localX = event.clientX - box.left;
		const localY = event.clientY - box.top;

		layerViewer.ScaleRelative(Math.pow(1.03, -delta * 1.5), localX, localY);
		layerViewer.RenderScene(true);
	}

	let dragging = false;
	let dragX = 0, dragY = 0;
	// Live machine/build-plate coordinates under the cursor (mm), shown bottom-right.
	let mousePosition = $state<{ x: number; y: number } | null>(null);

	// "Properties" hover inspector: when enabled, hovering a hatch/polyline shows
	// a popup with that segment's laser power, speed, profile, etc.
	type SegmentProperties = {
		type?: string;
		laserpower?: number;
		laserspeed?: number;
		profilename?: string;
		partid?: number;
		laserindex?: number;
		lineIndex?: number;
	};
	let propertiesMode = $state(false);
	let hoverSegment = $state<SegmentProperties | null>(null);
	let hoverScreen = $state<{ x: number; y: number }>({ x: 0, y: 0 });
	let pendingHover: { x: number; y: number } | null = null;
	let hoverRAF = 0;

	function updateMousePosition(event: PointerEvent) {
		if (!containerEl || !layerViewer || typeof layerViewer.screenToMachine !== 'function') return;
		const box = containerEl.getBoundingClientRect();
		mousePosition = layerViewer.screenToMachine(event.clientX - box.left, event.clientY - box.top);
	}

	function updateHoverSegment(clientX: number, clientY: number) {
		if (!propertiesMode || dragging || !containerEl || !layerViewer ||
			typeof layerViewer.pickSegmentAtScreenPoint !== 'function') {
			hoverSegment = null;
			return;
		}
		const box = containerEl.getBoundingClientRect();
		const localX = clientX - box.left;
		const localY = clientY - box.top;
		const result: SegmentProperties | null = layerViewer.pickSegmentAtScreenPoint(localX, localY, 6) ?? null;
		hoverSegment = result;
		hoverScreen = { x: localX, y: localY };

		if (typeof layerViewer.setHighlightLine === 'function') {
			layerViewer.setHighlightLine(result ? (result.lineIndex ?? -1) : -1);
		}
	}

	// Segment picking scans every line of the layer, so throttle it to one hit
	// test per animation frame regardless of how fast pointer events arrive.
	function scheduleHoverUpdate(clientX: number, clientY: number) {
		pendingHover = { x: clientX, y: clientY };
		if (hoverRAF) return;
		hoverRAF = requestAnimationFrame(() => {
			hoverRAF = 0;
			if (pendingHover) updateHoverSegment(pendingHover.x, pendingHover.y);
		});
	}

	function clearHover() {
		pendingHover = null;
		if (hoverRAF) {
			cancelAnimationFrame(hoverRAF);
			hoverRAF = 0;
		}
		hoverSegment = null;
		if (layerViewer && typeof layerViewer.clearHighlight === 'function') {
			layerViewer.clearHighlight();
		}
	}

	function togglePropertiesMode() {
		propertiesMode = !propertiesMode;
		if (!propertiesMode) clearHover();
	}

	function onPointerDown(event: PointerEvent) {
		if (event.button === 0 || event.button === 1) {
			dragging = true;
			clearHover();
			dragX = event.clientX;
			dragY = event.clientY;
			(event.target as HTMLElement).setPointerCapture(event.pointerId);
		}
	}

	function onPointerMove(event: PointerEvent) {
		updateMousePosition(event);

		if (dragging && layerViewer) {
			const dx = event.clientX - dragX;
			const dy = event.clientY - dragY;
			dragX = event.clientX;
			dragY = event.clientY;
			layerViewer.Drag(dx, dy);
			layerViewer.RenderScene(true);
			return;
		}

		if (propertiesMode) scheduleHoverUpdate(event.clientX, event.clientY);
	}

	function onPointerUp() {
		dragging = false;
	}

	function onPointerLeave() {
		mousePosition = null;
		clearHover();
	}

	// Frames the build-area rectangle. The origin is the location of machine-zero
	// inside the plate (measured from the lower-left corner), so the plate corners in
	// machine coordinates run from -origin to (size - origin). For origin=(sx/2,sy/2)
	// this yields a view symmetric around zero, e.g. [-100..100] x [-125..125].
	function centerOnPlatform() {
		if (!layerViewer || !platform) return;
		const ox = platform.originx || 0;
		const oy = platform.originy || 0;
		const sx = platform.sizex || 300;
		const sy = platform.sizey || 300;
		// Optional per-axis padding (in mm) that enlarges the reset zoom window,
		// added on top of the fixed ZOOM_MARGIN on every side.
		const px = platform.paddingx || 0;
		const py = platform.paddingy || 0;
		layerViewer.CenterOnRectangle(
			-ox - ZOOM_MARGIN - px, -oy - ZOOM_MARGIN - py,
			(sx - ox) + ZOOM_MARGIN + px, (sy - oy) + ZOOM_MARGIN + py
		);
	}

	function resetView() {
		if (!layerViewer || !platform) return;
		centerOnPlatform();
		layerViewer.RenderScene(true);
	}

	function fitToPath() {
		if (!layerViewer) return;
		try {
			const bounds = layerViewer.getPathBoundaries?.();
			if (bounds && bounds.radius > 0 && platform) {
				const left = bounds.center.x - bounds.radius + (platform.originx || 0);
				const right = bounds.center.x + bounds.radius + (platform.originx || 0);
				const top = bounds.center.y - bounds.radius + (platform.originy || 0);
				const bottom = bounds.center.y + bounds.radius + (platform.originy || 0);
				layerViewer.CenterOnRectangle(left, top, right, bottom);
			} else {
				resetView();
			}
			layerViewer.RenderScene(true);
		} catch { resetView(); }
	}

	onMount(() => {
		module.onDataHasChanged = onDataChanged;

		if (platform) {
			platform.displayed_layer = 0;
			platform.displayed_build = 0;
		}

		requestAnimationFrame(() => {
			ensureInit();
		});

		const ro = new ResizeObserver(() => {
			if (!initialized) {
				ensureInit();
			} else if (layerViewer && containerEl) {
				const w = containerEl.clientWidth, h = containerEl.clientHeight;
				if (w > 0 && h > 0) {
					layerViewer.updateSize(w, h);
					layerViewer.RenderScene(true);
				}
			}
		});
		if (containerEl) ro.observe(containerEl);

		return () => ro.disconnect();
	});

	onDestroy(() => {
		module.onDataHasChanged = null;
		clearHover();
		if (platform) {
			platform.displayed_layer = 0;
			platform.displayed_build = 0;
		}
	});
</script>

{#snippet layerViewBody()}
	<div class="layerview-container">
		<!-- WebGL render target — setupDOMElement sets position:relative on this -->
		<div
			bind:this={containerEl}
			class="layerview-canvas"
			role="img"
			onwheel={onWheel}
			onpointerdown={onPointerDown}
			onpointermove={onPointerMove}
			onpointerup={onPointerUp}
			onpointercancel={onPointerUp}
			onpointerleave={onPointerLeave}
		></div>

		<!-- Overlaid toolbar -->
		<div class="layerview-toolbar">
			<button class="layerview-btn" onclick={resetView} title="Frame the build platform" aria-label="Frame the build platform">
				<Square size={16} />
				<span>Platform</span>
			</button>
			<button class="layerview-btn" onclick={fitToPath} title="Frame the parts" aria-label="Frame the parts">
				<Shapes size={16} />
				<span>Parts</span>
			</button>
			<button
				class="layerview-btn"
				onclick={() => coordinateSystemOverride = !coordinateSystemVisible}
				title="Toggle coordinate axes"
				aria-label="Toggle coordinate axes"
				aria-pressed={coordinateSystemVisible}
			>
				<Axis3d size={16} />
				<span>Axes</span>
			</button>
			<button
				class="layerview-btn"
				onclick={togglePropertiesMode}
				title="Show segment properties on hover"
				aria-label="Toggle segment properties inspector"
				aria-pressed={propertiesMode}
			>
				<Info size={16} />
				<span>Properties</span>
			</button>
		</div>

		<!-- Layer info overlay -->
		{#if layerCount > 0}
			<div class="layerview-layer-info">
				Layer {sliderValue} / {layerCount}
			</div>
		{/if}

		{#if coordinateSystemVisible}
			<svg
				class="layerview-coordinate-indicator"
				viewBox="0 0 64 64"
				role="img"
				aria-label={`Machine coordinate axes, rotated ${transformAngle} degrees`}
			>
				<g transform={`translate(32 32) rotate(${-transformAngle})`}>
					<line class="coordinate-axis-x" x1="0" y1="0" x2="24" y2="0" />
					<polygon class="coordinate-axis-x" points="24,0 18,-3 18,3" />
					<text
						class="coordinate-label-x"
						x="27"
						y="4"
						transform={`rotate(${transformAngle} 27 4)`}
					>X</text>

					<line class="coordinate-axis-y" x1="0" y1="0" x2="0" y2="-24" />
					<polygon class="coordinate-axis-y" points="0,-24 -3,-18 3,-18" />
					<text
						class="coordinate-label-y"
						x="4"
						y="-24"
						transform={`rotate(${transformAngle} 4 -24)`}
					>Y</text>
				</g>
			</svg>
		{/if}

		<!-- Live cursor position readout (machine coordinates, mm) -->
		{#if mousePosition}
			<div class="layerview-mouse-pos">
				X: {mousePosition.x.toFixed(2)} &middot; Y: {mousePosition.y.toFixed(2)} mm
			</div>
		{/if}

		<!-- Segment property inspector popup (Properties toggle) -->
		{#if propertiesMode && hoverSegment}
			<div
				class="layerview-segment-popup"
				style={`left: ${hoverScreen.x + 14}px; top: ${hoverScreen.y + 14}px;`}
			>
				{#if hoverSegment.profilename}
					<div class="layerview-segment-popup-title">{hoverSegment.profilename}</div>
				{/if}
				<dl class="layerview-segment-popup-list">
					<dt>Laser power</dt>
					<dd>{Number(hoverSegment.laserpower ?? 0).toLocaleString()} W</dd>
					<dt>Laser speed</dt>
					<dd>{Number(hoverSegment.laserspeed ?? 0).toLocaleString()} mm/s</dd>
					{#if hoverSegment.type}
						<dt>Type</dt>
						<dd>{hoverSegment.type}</dd>
					{/if}
					{#if hoverSegment.laserindex !== undefined && hoverSegment.laserindex !== null}
						<dt>Laser</dt>
						<dd>#{hoverSegment.laserindex}</dd>
					{/if}
					{#if hoverSegment.partid !== undefined && hoverSegment.partid !== null}
						<dt>Part ID</dt>
						<dd>{hoverSegment.partid}</dd>
					{/if}
				</dl>
			</div>
		{/if}

		<!-- Layer slider (vertical) -->
		{#if layerCount > 0}
			<div class="layerview-slider-wrap">
				<input
					type="range"
					class="layerview-slider"
					min="0"
					max={layerCount}
					value={sliderValue}
					oninput={onSliderChange}
					aria-label="Layer"
					aria-orientation="vertical"
				/>
			</div>
		{/if}
	</div>
{/snippet}

{#if visible}
	{#if isCard}
		<Card.Root class="flex flex-col h-full min-h-0">
			{#if cardTitle}
				<Card.Header class="pb-1">
					<Card.Title>{cardTitle}</Card.Title>
					{#if cardSubtitle}
						<Card.Description>{cardSubtitle}</Card.Description>
					{/if}
				</Card.Header>
			{/if}
			<Card.Content class="flex-1 min-h-0 overflow-hidden flex flex-col">
				{@render layerViewBody()}
			</Card.Content>
		</Card.Root>
	{:else}
		{@render layerViewBody()}
	{/if}
{/if}

<style>
	.layerview-container {
		position: relative;
		width: 100%;
		height: 100%;
		overflow: hidden;
	}
	.layerview-canvas {
		width: 100%;
		height: 100%;
		cursor: crosshair;
	}
	.layerview-toolbar {
		position: absolute;
		top: 8px;
		left: 8px;
		display: flex;
		gap: 4px;
		z-index: 10;
	}
	.layerview-btn {
		display: inline-flex;
		flex-direction: column;
		align-items: center;
		justify-content: center;
		gap: 4px;
		width: 64px;
		height: 48px;
		padding: 6px 4px;
		border: none;
		border-radius: 4px;
		background: rgba(0, 0, 0, 0.65);
		color: white;
		font-size: 11px;
		cursor: pointer;
		transition: background-color 0.2s;
	}
	.layerview-btn:hover {
		background-color: rgba(0, 0, 0, 0.85);
	}
	.layerview-btn[aria-pressed='true'] {
		background-color: var(--primary, #2563eb);
		box-shadow: 0 0 0 1px rgba(255, 255, 255, 0.65);
	}
	.layerview-layer-info {
		position: absolute;
		top: 8px;
		right: 8px;
		padding: 4px 10px;
		border-radius: 4px;
		background: rgba(0, 0, 0, 0.65);
		color: white;
		font-size: 11px;
		font-variant-numeric: tabular-nums;
		z-index: 10;
	}
	.layerview-coordinate-indicator {
		position: absolute;
		left: 8px;
		bottom: 8px;
		width: 64px;
		height: 64px;
		pointer-events: none;
		z-index: 9;
	}
	.coordinate-axis-x {
		fill: #ef4444;
		stroke: #ef4444;
		stroke-width: 2;
	}
	.coordinate-axis-y {
		fill: #22c55e;
		stroke: #22c55e;
		stroke-width: 2;
	}
	.coordinate-label-x,
	.coordinate-label-y {
		font-size: 11px;
		font-weight: 600;
		text-anchor: middle;
		stroke: none;
	}
	.coordinate-label-x {
		fill: #ef4444;
	}
	.coordinate-label-y {
		fill: #22c55e;
	}
	.layerview-mouse-pos {
		position: absolute;
		/* Shifted left so it clears the vertical layer slider on the right edge. */
		right: 40px;
		bottom: 8px;
		padding: 4px 10px;
		border-radius: 4px;
		background: rgba(0, 0, 0, 0.65);
		color: white;
		font-size: 11px;
		font-variant-numeric: tabular-nums;
		pointer-events: none;
		z-index: 10;
	}
	.layerview-segment-popup {
		position: absolute;
		min-width: 150px;
		max-width: 240px;
		padding: 8px 10px;
		border-radius: 6px;
		background: rgba(0, 0, 0, 0.82);
		color: white;
		font-size: 11px;
		line-height: 1.35;
		pointer-events: none;
		z-index: 20;
		box-shadow: 0 4px 12px rgba(0, 0, 0, 0.35);
	}
	.layerview-segment-popup-title {
		font-weight: 600;
		margin-bottom: 4px;
		overflow: hidden;
		text-overflow: ellipsis;
		white-space: nowrap;
	}
	.layerview-segment-popup-list {
		display: grid;
		grid-template-columns: auto auto;
		gap: 2px 12px;
		margin: 0;
	}
	.layerview-segment-popup-list dt {
		color: rgba(255, 255, 255, 0.65);
	}
	.layerview-segment-popup-list dd {
		margin: 0;
		text-align: right;
		font-variant-numeric: tabular-nums;
	}
	.layerview-slider-wrap {
		position: absolute;
		right: 8px;
		/* Anchor between the layer-info badge (top-right) and the bottom edge
		   so the slider spans nearly the full height of the view. */
		top: 44px;
		bottom: 16px;
		display: flex;
		align-items: center;
		justify-content: center;
		z-index: 10;
	}
	.layerview-slider {
		writing-mode: vertical-lr;
		direction: rtl;
		height: 100%;
		width: 20px;
		accent-color: var(--primary, #2563eb);
	}
</style>
