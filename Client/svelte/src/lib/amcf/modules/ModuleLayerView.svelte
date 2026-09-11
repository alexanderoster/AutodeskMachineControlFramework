<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import { usePollTick } from '$lib/amcf/poll.svelte';
	// @ts-ignore — core JS has no type declarations yet
	import WebGLImpl from '@core/common/AMCImplementation_WebGL.js';
	// @ts-ignore
	import LayerViewImpl from '@core/common/AMCImplementation_LayerView.js';

	const ZOOM_MARGIN = 10;

	let { module, app }: { module: any; app: any } = $props();
	const poll = usePollTick();

	let visible = $derived.by(() => { poll.v; return module.visible !== false; });
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
				layerViewer.CenterOnRectangle(
					-ZOOM_MARGIN, -ZOOM_MARGIN,
					(platform.sizex || 300) + ZOOM_MARGIN,
					(platform.sizey || 300) + ZOOM_MARGIN
				);

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

	function onPointerDown(event: PointerEvent) {
		if (event.button === 0 || event.button === 1) {
			dragging = true;
			dragX = event.clientX;
			dragY = event.clientY;
			(event.target as HTMLElement).setPointerCapture(event.pointerId);
		}
	}

	function onPointerMove(event: PointerEvent) {
		if (!dragging || !layerViewer) return;
		const dx = event.clientX - dragX;
		const dy = event.clientY - dragY;
		dragX = event.clientX;
		dragY = event.clientY;
		layerViewer.Drag(dx, dy);
		layerViewer.RenderScene(true);
	}

	function onPointerUp() {
		dragging = false;
	}

	function resetView() {
		if (!layerViewer || !platform) return;
		layerViewer.CenterOnRectangle(
			-ZOOM_MARGIN, -ZOOM_MARGIN,
			(platform.sizex || 300) + ZOOM_MARGIN,
			(platform.sizey || 300) + ZOOM_MARGIN
		);
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
		if (platform) {
			platform.displayed_layer = 0;
			platform.displayed_build = 0;
		}
	});
</script>

{#if visible}
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
		></div>

		<!-- Overlaid toolbar -->
		<div class="layerview-toolbar">
			<button class="layerview-btn" onclick={resetView}>Reset View</button>
			<button class="layerview-btn" onclick={fitToPath}>Fit</button>
			<button
				class="layerview-btn"
				onclick={() => coordinateSystemOverride = !coordinateSystemVisible}
				title="Toggle coordinate axes"
				aria-label="Toggle coordinate axes"
				aria-pressed={coordinateSystemVisible}
			>Axes</button>
		</div>

		<!-- Layer info overlay -->
		{#if layerCount > 0}
			<div class="layerview-layer-info">
				Layer {sliderValue} / {layerCount}
			</div>
		{/if}

		{#if coordinateSystemVisible}
			<svg
				class={['layerview-coordinate-indicator', { 'above-slider': layerCount > 0 }]}
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

		<!-- Layer slider -->
		{#if layerCount > 0}
			<div class="layerview-slider-wrap">
				<input
					type="range"
					class="layerview-slider"
					min="0"
					max={layerCount}
					value={sliderValue}
					oninput={onSliderChange}
				/>
			</div>
		{/if}
	</div>
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
		padding: 4px 12px;
		border: none;
		border-radius: 4px;
		background: rgba(0, 0, 0, 0.65);
		color: white;
		font-size: 12px;
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
		border-radius: 4px;
		background: rgba(0, 0, 0, 0.35);
		pointer-events: none;
		z-index: 9;
	}
	.layerview-coordinate-indicator.above-slider {
		bottom: 36px;
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
	.layerview-slider-wrap {
		position: absolute;
		bottom: 8px;
		left: 8px;
		right: 8px;
		z-index: 10;
	}
	.layerview-slider {
		width: 100%;
		accent-color: var(--primary, #2563eb);
	}
</style>
