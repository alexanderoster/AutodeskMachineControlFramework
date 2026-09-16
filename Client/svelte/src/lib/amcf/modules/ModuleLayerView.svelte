<script lang="ts">
	import { onMount, onDestroy, untrack } from 'svelte';
	import { usePollTick } from '$lib/amcf/poll.svelte';
	import MdiIcon from '$lib/amcf/MdiIcon.svelte';
	import Square from '@lucide/svelte/icons/square';
	import SquareCheck from '@lucide/svelte/icons/square-check';
	import Palette from '@lucide/svelte/icons/palette';
	import Minus from '@lucide/svelte/icons/minus';
	import Plus from '@lucide/svelte/icons/plus';
	import LoaderCircle from '@lucide/svelte/icons/loader-circle';
	// @ts-ignore — core JS has no type declarations yet
	import WebGLImpl from '@core/common/AMCImplementation_WebGL.js';
	// @ts-ignore
	import LayerViewImpl from '@core/common/AMCImplementation_LayerView.js';

	const ZOOM_MARGIN = 10;
	const NULL_UUID = '00000000-0000-0000-0000-000000000000';
	// Columns of the "laser" scatter plot channel that LayerViewImpl evaluates
	const LASER_CHANNEL_COLUMNS = ['laseron', 'power'];

	// Point color modes in toggle order, see LayerViewImpl.updateColors ()
	const COLOR_MODES = ['time', 'velocity', 'laseron', 'powerramp', 'uniform'];
	const COLOR_MODE_CAPTIONS: Record<string, string> = {
		time: 'Timing',
		velocity: 'Velocity',
		laseron: 'LaserOn',
		powerramp: 'Power',
		uniform: 'Uniform'
	};

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
	let sliderFixed = $derived.by(() => { poll.v; return !!platform?.sliderfixed; });
	let labelVisible = $derived.by(() => { poll.v; return !!platform?.labelvisible; });
	let labelCaption = $derived.by(() => { poll.v; return platform?.labelcaption || ''; });
	let labelIcon = $derived.by(() => { poll.v; return platform?.labelicon || ''; });
	let sliderValue = $state(0);
	let appliedColorTheme = $state('');
	let coordinateSystemOverride: boolean | null = $state(null);
	let coordinateSystemVisible = $derived(
		coordinateSystemOverride ?? Boolean(platform?.showcoordinatesystem)
	);
	let appliedCoordinateTransform: CoordinateTransform | null = null;

	let loadingLayer = $state(false);
	let loadingPoints = $state(false);
	let pointsAvailable = $state(false);
	let toolpathVisible = $state(true);
	let showLaserOffPoints = $state(false);
	let colorMode = $state('uniform');
	let hoverInfo = $state({ visible: false, text: '', x: 0, y: 0, flipX: false, flipY: false });

	// Scatter plot that is shown or being loaded. It is tracked apart from the layer index, because
	// after a layer change the server reports the new index before the slider change event has
	// computed the scatter plot of that layer.
	let displayedScatterplot = '';
	let lastServerLayer: number | null = null;
	let draggingSlider = false;
	let hoverFrame = 0;
	let hoverClientX = 0;
	let hoverClientY = 0;

	function isValidUUID (uuid: string): boolean {
		return !!uuid && (uuid !== NULL_UUID);
	}

	// Follow the layer of the server, unless the slider is being dragged. Only a changed server value
	// is applied, so the slider does not jump back while a layer change is still being processed.
	$effect(() => {
		poll.v;
		if (!platform) return;
		const serverLayer = platform.currentlayer || 0;
		if (serverLayer !== lastServerLayer) {
			lastServerLayer = serverLayer;
			if (!draggingSlider)
				sliderValue = serverLayer;
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
		if (isDark && isValidUUID(platform.dark_baseimageresource))
			return app.getImageURL(platform.dark_baseimageresource);
		if (isValidUUID(platform.baseimageresource))
			return app.getImageURL(platform.baseimageresource);
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

	// Runs on mount and whenever the container reappears: hiding the module destroys the container
	// together with the canvas that three.js appended to it.
	function ensureInit() {
		if (!containerEl || !app) return;
		const w = containerEl.clientWidth, h = containerEl.clientHeight;
		if (w === 0 || h === 0) return;

		let firstInit = false;

		try {
			if (!glInstance) {
				glInstance = app.retrieveWebGLInstance(module.uuid);
				if (!glInstance) {
					glInstance = new WebGLImpl();
					app.storeWebGLInstance(module.uuid, glInstance);
				}
			}

			if (!layerViewer) {
				layerViewer = new LayerViewImpl(glInstance);
				firstInit = true;
			}

			// Re-attach the canvas when the container has been recreated
			if (!containerEl.contains(glInstance.renderer.domElement))
				glInstance.setupDOMElement(containerEl);
			layerViewer.updateSize(w, h);
			layerViewer.setCoordinateTransform(...coordinateTransform);
			appliedCoordinateTransform = coordinateTransform;

			if (firstInit && platform) {
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
			return;
		}

		// Load the current layer right away instead of waiting for the next poll
		if (firstInit) {
			displayedScatterplot = '';
			onDataChanged(module);
		}
	}

	function onDataChanged(sender: any) {
		const currentPlatform = module.platform;
		if (!layerViewer || !currentPlatform || !sender) return;
		if (!module.isActive || !module.isActive()) return;
		if (sender.uuid !== module.uuid) return;

		if (currentPlatform.displayed_layer !== currentPlatform.currentlayer ||
			currentPlatform.displayed_build !== currentPlatform.builduuid) {

			currentPlatform.displayed_layer = currentPlatform.currentlayer;
			currentPlatform.displayed_build = currentPlatform.builduuid;
			loadToolpath(currentPlatform.builduuid, currentPlatform.currentlayer);
		}

		const scatterplotUUID = currentPlatform.scatterplotuuid || NULL_UUID;
		if (scatterplotUUID !== displayedScatterplot)
			loadScatterplot(scatterplotUUID);
	}

	function isDisplayedToolpath(buildUUID: string, layerIndex: number): boolean {
		const currentPlatform = module.platform;
		return !!currentPlatform && (currentPlatform.displayed_build === buildUUID) && (currentPlatform.displayed_layer === layerIndex);
	}

	function loadToolpath(buildUUID: string, layerIndex: number) {
		if (!layerViewer) return;
		hideHoverInfo();

		if (!isValidUUID(buildUUID)) {
			loadingLayer = false;
			layerViewer.loadLayer(null);
			return;
		}

		loadingLayer = true;
		app.axiosPostRequest('/build/toolpath', {
			builduuid: buildUUID,
			layerindex: layerIndex
		})
		.then((layerJSON: any) => {
			// A newer layer has been requested in the meantime
			if (!layerViewer || !isDisplayedToolpath(buildUUID, layerIndex)) return;
			layerViewer.loadLayer(layerJSON.data.segments);
		})
		.catch((err: any) => {
			console.warn('[LayerView] layer load error:', err?.response || err);
			if (layerViewer) layerViewer.RenderScene(true);
		})
		.finally(() => {
			if (isDisplayedToolpath(buildUUID, layerIndex)) loadingLayer = false;
		});
	}

	function clearPoints() {
		if (!layerViewer) return;
		layerViewer.clearPointsChannelData('laser');
		layerViewer.clearPoints();
		pointsAvailable = false;
		hideHoverInfo();
		layerViewer.RenderScene(true);
	}

	// Loads the point positions of a scatter plot and the columns of its "laser" channel (laseron, power)
	function loadScatterplot(scatterplotUUID: string) {
		displayedScatterplot = scatterplotUUID;
		clearPoints();

		if (!isValidUUID(scatterplotUUID)) {
			loadingPoints = false;
			return;
		}

		loadingPoints = true;
		Promise.all([
			app.axiosGetArrayBufferRequest('/pointcloud/' + scatterplotUUID),
			app.axiosGetRequest('/pointchanneldata/' + scatterplotUUID + '/laser', { timeout: 0 })
		])
		.then(([pointsResponse, channelResponse]: any[]) => {
			// Another scatter plot has been requested in the meantime
			if (!layerViewer || (scatterplotUUID !== displayedScatterplot)) return;

			layerViewer.loadPoints(new Float32Array(pointsResponse.data));

			// Besides one array per column, the response contains the protocol header fields.
			// Only the columns the viewer knows are loaded, like in the Vue 2 client.
			const channelData = channelResponse.data || {};
			for (const key of Object.keys(channelData)) {
				const column = key.toLowerCase();
				if (LASER_CHANNEL_COLUMNS.includes(column) && Array.isArray(channelData[key]))
					layerViewer.loadPointsChannelData('laser', column, new Float32Array(channelData[key]));
			}

			layerViewer.updateColors();
			layerViewer.updateLayerPoints();
			pointsAvailable = layerViewer.pointDataIsAvailable();
		})
		.catch((err: any) => {
			console.warn('[LayerView] scatter plot load error:', err?.response || err);
			if (scatterplotUUID === displayedScatterplot) clearPoints();
		})
		.finally(() => {
			if (scatterplotUUID === displayedScatterplot) loadingPoints = false;
		});
	}

	function toggleToolpath() {
		if (!layerViewer) return;
		toolpathVisible = !toolpathVisible;
		layerViewer.toolpathVisible = toolpathVisible;
		layerViewer.updateLoadedLayer();
		hideHoverInfo();
	}

	function toggleLaserOffPoints() {
		if (!layerViewer) return;
		showLaserOffPoints = !showLaserOffPoints;
		layerViewer.showLaserOffPoints = showLaserOffPoints;
		layerViewer.updateLayerPoints();
		layerViewer.RenderScene(true);
		hideHoverInfo();
	}

	function cycleColorMode() {
		if (!layerViewer) return;
		colorMode = COLOR_MODES[(COLOR_MODES.indexOf(colorMode) + 1) % COLOR_MODES.length];
		layerViewer.setColorMode(colorMode);
	}

	function changeLayerTo(targetLayer: number) {
		if (!app || !platform || sliderFixed) return;
		if (isNaN(targetLayer) || (targetLayer < 0) || (targetLayer >= layerCount)) return;

		sliderValue = targetLayer;
		if (targetLayer !== platform.currentlayer) {
			// The slider moves optimistically. If the request fails, the server keeps its layer and the
			// follow-server effect will not fire (the value did not change), so roll back here.
			app.triggerWidgetRequest(platform.uuid, 'changelayer', { targetlayer: targetLayer }, undefined, (err: any) => {
				if (!draggingSlider && (sliderValue === targetLayer))
					sliderValue = module.platform?.currentlayer || 0;
				app.showSnackBar('Layer change to ' + targetLayer + ' failed: ' + app.extractErrorMessage(err), 'error', 8000);
			});
		}
	}

	// Every layer change recomputes the scatter plot on the server, so the layer is only sent when
	// the slider is released and not for every intermediate position.
	function onSliderInput(e: Event) {
		draggingSlider = true;
		sliderValue = parseInt((e.target as HTMLInputElement).value);
	}

	function onSliderChange(e: Event) {
		draggingSlider = false;
		changeLayerTo(parseInt((e.target as HTMLInputElement).value));
	}

	function onSliderRelease() {
		draggingSlider = false;
	}

	function hideHoverInfo() {
		// A frame that is already scheduled would show the tooltip again
		if (hoverFrame) {
			cancelAnimationFrame(hoverFrame);
			hoverFrame = 0;
		}
		if (hoverInfo.visible)
			hoverInfo.visible = false;
	}

	function describePoint(mouseX: number, mouseY: number): string {
		if (!pointsAvailable) return '';
		const pointIndex = layerViewer.resolvePointIndex(glInstance.getRaycasterCollisions('layerdata_points', mouseX, mouseY));
		if (pointIndex < 0) return '';

		let text = `Point ID = ${pointIndex}`;
		const position = layerViewer.getPointPosition(pointIndex);
		if (position) text += `\nPosition: ${position.x.toFixed(4)} / ${position.y.toFixed(4)} mm`;
		const velocity = layerViewer.getPointVelocity(pointIndex);
		if (velocity > 0) text += `\nVelocity: ${velocity.toFixed(4)} mm/s`;
		const acceleration = layerViewer.getPointAcceleration(pointIndex);
		if (acceleration) text += `\nAcceleration: ${(acceleration.a / 1000).toFixed(4)} m/s²`;
		const jerk = layerViewer.getPointJerk(pointIndex);
		if (jerk) text += `\nJerk: ${(jerk.j / 1000000).toFixed(4)} km/s³`;
		const power = layerViewer.getPointPower(pointIndex);
		if (power !== null) text += `\nPower: ${power.toFixed(4)} W`;
		return text;
	}

	function describeSegment(mouseX: number, mouseY: number): string {
		if (!toolpathVisible) return '';
		const lineIndex = glInstance.getRaycasterCollisions('layerdata_lines', mouseX, mouseY);
		const coordinates = layerViewer.linesCoordinates;
		if ((lineIndex < 0) || !coordinates || (lineIndex * 4 + 3 >= coordinates.length)) return '';

		const [x1, y1, x2, y2] = coordinates.slice(lineIndex * 4, lineIndex * 4 + 4);
		let text = `Line ID = ${lineIndex}\n${x1.toFixed(3)} / ${y1.toFixed(3)} - ${x2.toFixed(3)} / ${y2.toFixed(3)} mm`;
		const properties = layerViewer.segmentProperties?.[lineIndex];
		if (properties) {
			if ((typeof properties.laserpower === 'number') && (typeof properties.laserspeed === 'number'))
				text += `\n${properties.laserpower.toFixed(0)} W / ${properties.laserspeed.toFixed(1)} mm/s`;
			if (properties.profilename)
				text += `\nProfile: ${properties.profilename}`;
		}
		return text;
	}

	// Raycasting is expensive for large layers, so it runs at most once per animation frame
	function scheduleHoverUpdate(event: PointerEvent) {
		hoverClientX = event.clientX;
		hoverClientY = event.clientY;
		if (!hoverFrame)
			hoverFrame = requestAnimationFrame(updateHoverInfo);
	}

	function updateHoverInfo() {
		hoverFrame = 0;
		if (!glInstance?.renderer || !layerViewer || !containerEl || dragging) return;

		const canvasBox = glInstance.renderer.domElement.getBoundingClientRect();
		const mouseX = hoverClientX - canvasBox.left;
		const mouseY = hoverClientY - canvasBox.top;
		const text = describePoint(mouseX, mouseY) || describeSegment(mouseX, mouseY);
		if (!text) {
			hideHoverInfo();
			return;
		}

		const containerBox = containerEl.getBoundingClientRect();
		const x = hoverClientX - containerBox.left;
		const y = hoverClientY - containerBox.top;
		hoverInfo.text = text;
		hoverInfo.x = x;
		hoverInfo.y = y;
		hoverInfo.flipX = x > containerBox.width / 2;
		hoverInfo.flipY = y > containerBox.height / 2;
		hoverInfo.visible = true;
	}

	function onWheel(event: WheelEvent) {
		if (!containerEl || !layerViewer) return;
		event.preventDefault();
		hideHoverInfo();

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
			hideHoverInfo();
		}
	}

	function onPointerMove(event: PointerEvent) {
		if (!dragging) {
			scheduleHoverUpdate(event);
			return;
		}
		if (!layerViewer) return;
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

	// The container is recreated whenever the module is hidden and shown again, so the canvas has to be
	// re-attached and the resize observer re-registered. ensureInit () reads and writes state that must
	// not become a dependency of this effect.
	$effect(() => {
		const element = containerEl;
		if (!element) return;

		untrack(() => ensureInit());

		const observer = new ResizeObserver(() => {
			const w = element.clientWidth, h = element.clientHeight;
			if ((w === 0) || (h === 0)) return;

			if (!layerViewer || !glInstance?.renderer || !element.contains(glInstance.renderer.domElement)) {
				ensureInit();
				return;
			}

			layerViewer.updateSize(w, h);
			layerViewer.RenderScene(true);
		});
		observer.observe(element);

		return () => observer.disconnect();
	});

	onMount(() => {
		module.onDataHasChanged = onDataChanged;

		if (platform) {
			platform.displayed_layer = 0;
			platform.displayed_build = 0;
		}
	});

	onDestroy(() => {
		module.onDataHasChanged = null;
		if (hoverFrame) cancelAnimationFrame(hoverFrame);
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
			onpointerleave={hideHoverInfo}
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
			<button class="layerview-btn" onclick={toggleToolpath}>
				{#if toolpathVisible}<SquareCheck class="h-3.5 w-3.5" />{:else}<Square class="h-3.5 w-3.5" />{/if}
				Toolpath
			</button>
			{#if pointsAvailable}
				<button class="layerview-btn" onclick={cycleColorMode}>
					<Palette class="h-3.5 w-3.5" />
					Color: {COLOR_MODE_CAPTIONS[colorMode] || 'Uniform'}
				</button>
				<button class="layerview-btn" onclick={toggleLaserOffPoints}>
					{#if showLaserOffPoints}<SquareCheck class="h-3.5 w-3.5" />{:else}<Square class="h-3.5 w-3.5" />{/if}
					Show LaserOff Points
				</button>
			{/if}
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

		<!-- Info about the point or toolpath segment under the mouse -->
		{#if hoverInfo.visible}
			<div
				class="layerview-hover-info"
				style="left: {hoverInfo.x}px; top: {hoverInfo.y}px; transform: translate({hoverInfo.flipX ? 'calc(-100% - 12px)' : '12px'}, {hoverInfo.flipY ? 'calc(-100% - 12px)' : '12px'});"
			>{hoverInfo.text}</div>
		{/if}

		<div class="layerview-bottom">
			{#if (labelVisible && (labelCaption || labelIcon)) || loadingLayer || loadingPoints}
				<div class="layerview-status">
					{#if labelVisible && (labelCaption || labelIcon)}
						<div class="layerview-label">
							<MdiIcon icon={labelIcon} class="h-3.5 w-3.5" />
							<span>{labelCaption}</span>
						</div>
					{/if}
					{#if loadingLayer || loadingPoints}
						<div class="layerview-label">
							<LoaderCircle class="h-3.5 w-3.5 animate-spin" />
							<span>Loading layer data</span>
						</div>
					{/if}
				</div>
			{/if}

			<!-- Layer slider -->
			{#if layerCount > 0}
				<div class="layerview-slider-row">
					{#if !sliderFixed}
						<button class="layerview-btn layerview-step" onclick={() => changeLayerTo(sliderValue - 1)} disabled={sliderValue <= 0} title="Previous layer" aria-label="Previous layer">
							<Minus class="h-3.5 w-3.5" />
						</button>
					{/if}
					<input
						type="range"
						class="layerview-slider"
						min="0"
						max={layerCount - 1}
						value={sliderValue}
						disabled={sliderFixed}
						oninput={onSliderInput}
						onchange={onSliderChange}
						onpointerup={onSliderRelease}
					/>
					{#if !sliderFixed}
						<button class="layerview-btn layerview-step" onclick={() => changeLayerTo(sliderValue + 1)} disabled={sliderValue >= layerCount - 1} title="Next layer" aria-label="Next layer">
							<Plus class="h-3.5 w-3.5" />
						</button>
					{/if}
				</div>
			{/if}
		</div>
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
		max-width: calc(100% - 140px);
		display: flex;
		flex-wrap: wrap;
		gap: 4px;
		z-index: 10;
	}
	.layerview-btn {
		display: inline-flex;
		align-items: center;
		gap: 4px;
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
	.layerview-btn:disabled {
		opacity: 0.4;
		cursor: default;
	}
	.layerview-step {
		padding: 4px 6px;
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
	.layerview-hover-info {
		position: absolute;
		z-index: 20;
		padding: 5px 8px;
		border-radius: 4px;
		background: rgba(0, 0, 0, 0.75);
		color: white;
		font-size: 11px;
		font-variant-numeric: tabular-nums;
		white-space: pre-line;
		pointer-events: none;
	}
	.layerview-bottom {
		position: absolute;
		bottom: 8px;
		left: 8px;
		right: 8px;
		display: flex;
		flex-direction: column;
		gap: 6px;
		z-index: 10;
	}
	.layerview-status {
		display: flex;
		align-self: flex-start;
		gap: 4px;
	}
	.layerview-label {
		display: flex;
		align-items: center;
		gap: 6px;
		padding: 4px 10px;
		border-radius: 4px;
		background: rgba(0, 0, 0, 0.75);
		color: white;
		font-size: 11px;
		font-variant-numeric: tabular-nums;
	}
	.layerview-slider-row {
		display: flex;
		align-items: center;
		gap: 6px;
	}
	.layerview-slider {
		flex: 1;
		accent-color: var(--primary, #2563eb);
	}
</style>
