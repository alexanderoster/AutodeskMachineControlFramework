<script lang="ts">
	import { onMount, onDestroy, untrack } from 'svelte';
	import { usePollTick } from '$lib/amcf/poll.svelte';
	import * as Card from '$lib/components/ui/card/index.js';
	import MdiIcon from '$lib/amcf/MdiIcon.svelte';
	import Square from '@lucide/svelte/icons/square';
	import SquareCheck from '@lucide/svelte/icons/square-check';
	import Shapes from '@lucide/svelte/icons/shapes';
	import ZoomIn from '@lucide/svelte/icons/zoom-in';
	import Axis3d from '@lucide/svelte/icons/axis-3d';
	import Tags from '@lucide/svelte/icons/tags';
	import Info from '@lucide/svelte/icons/info';
	import Route from '@lucide/svelte/icons/route';
	import Palette from '@lucide/svelte/icons/palette';
	import Minus from '@lucide/svelte/icons/minus';
	import Plus from '@lucide/svelte/icons/plus';
	import LoaderCircle from '@lucide/svelte/icons/loader-circle';
	// @ts-ignore — core JS has no type declarations yet
	import WebGLImpl from '@core/common/AMCImplementation_WebGL.js';
	// @ts-ignore
	import LayerViewImpl from '@core/common/AMCImplementation_LayerView.js';

	const ZOOM_MARGIN = 10;
	// Smaller selections are treated as accidental clicks and do not zoom.
	const MIN_ZOOM_SELECTION_PX = 5;
	const PART_MARKER_PX = 5;
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
	let transformAngle = $derived.by(() => { poll.v; return Number(platform?.transformangle) || 0; });
	let rotationCenterX = $derived.by(() => { poll.v; return Number(platform?.rotationcenterx) || 0; });
	let rotationCenterY = $derived.by(() => { poll.v; return Number(platform?.rotationcentery) || 0; });
	let translationX = $derived.by(() => { poll.v; return Number(platform?.translationx) || 0; });
	let translationY = $derived.by(() => { poll.v; return Number(platform?.translationy) || 0; });
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
	let coordinateSystemVisible = $derived.by(() => {
		poll.v;
		return coordinateSystemOverride ?? Boolean(platform?.showcoordinatesystem);
	});
	let appliedCoordinateTransform: CoordinateTransform | null = null;
	// While true, the view keeps framing the platform whenever the viewport or the
	// platform geometry changes. Cleared once the user pans or zooms; set again by
	// the "Zoom to Platform" button.
	let autoFrame = true;
	let platformFrameKey = $derived.by(() => {
		poll.v;
		return platform
			? [platform.sizex, platform.sizey, platform.originx, platform.originy, platform.paddingx, platform.paddingy].join('|')
			: '';
	});

	let loadingLayer = $state(false);
	let loadingPoints = $state(false);
	let pointsAvailable = $state(false);
	let toolpathVisible = $state(true);
	let showLaserOffPoints = $state(false);
	let colorMode = $state('uniform');
	// Plain tooltip with the point or toolpath segment under the mouse (always on)
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

	$effect(() => {
		platformFrameKey;
		if (!initialized || !autoFrame) return;
		untrack(() => resetView());
	});

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
				// Write-only, so effects that pan or zoom do not subscribe to viewVersion.
				layerViewer.onTransformChanged = () => { viewVersion = ++transformChangeCount; };
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
				centerOnPlatform();

				platform.displayed_layer = 0;
				platform.displayed_build = 0;
			} else if (autoFrame) {
				centerOnPlatform();
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
			currentPlatform.displayed_build !== currentPlatform.builduuid ||
			currentPlatform.displayed_partstateversion !== currentPlatform.partstateversion) {

			currentPlatform.displayed_layer = currentPlatform.currentlayer;
			currentPlatform.displayed_build = currentPlatform.builduuid;
			currentPlatform.displayed_partstateversion = currentPlatform.partstateversion;
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
		clearHover();

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
			layerViewer.loadLayer(layerJSON.data.segments, layerJSON.data.parts);
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
		clearHover();
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

	// Clicking the "Layer x / n" badge swaps it for a number field; Enter jumps to the typed
	// layer (clamped to the slider range), Escape or leaving the field cancels.
	let layerJumpOpen = $state(false);
	let layerJumpValue = $state<number | null>(null);

	function openLayerJump() {
		if (sliderFixed) return;
		layerJumpValue = sliderValue;
		layerJumpOpen = true;
	}

	function commitLayerJump() {
		if (!layerJumpOpen) return;
		layerJumpOpen = false;
		if (typeof layerJumpValue !== 'number' || !Number.isFinite(layerJumpValue)) return;
		changeLayerTo(Math.min(Math.max(Math.round(layerJumpValue), 0), Math.max(layerCount - 1, 0)));
	}

	function onLayerJumpKeyDown(event: KeyboardEvent) {
		if (event.key === 'Enter') {
			event.preventDefault();
			commitLayerJump();
		} else if (event.key === 'Escape') {
			event.preventDefault();
			event.stopPropagation();
			layerJumpOpen = false;
		}
	}

	function focusAndSelect(node: HTMLInputElement) {
		node.focus();
		node.select();
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
			hoverFrame = requestAnimationFrame(updateHover);
	}

	function updateHover() {
		hoverFrame = 0;
		if (dragging) return;

		if (propertiesMode) {
			// The "Properties" inspector replaces the plain tooltip while it is active
			if (hoverInfo.visible) hoverInfo.visible = false;
			updateHoverSegment(hoverClientX, hoverClientY);
			return;
		}

		updateHoverInfo();
	}

	function updateHoverInfo() {
		if (!glInstance?.renderer || !layerViewer || !containerEl) return;

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
		clearHover();

		let delta = event.deltaY;
		if (delta > 5) delta = 5;
		if (delta < -5) delta = -5;

		const box = containerEl.getBoundingClientRect();
		const localX = event.clientX - box.left;
		const localY = event.clientY - box.top;

		autoFrame = false;
		layerViewer.ScaleRelative(Math.pow(1.03, -delta * 1.5), localX, localY);
		layerViewer.RenderScene(true);
	}

	let dragging = false;
	let dragX = 0, dragY = 0;

	// "Custom Zoom": while active, a left-button drag draws a selection rectangle
	// (viewport pixels) instead of panning; releasing it frames that rectangle.
	let zoomSelectMode = $state(false);
	let zoomSelection = $state<{ startX: number; startY: number; endX: number; endY: number } | null>(null);
	let zoomRect = $derived(zoomSelection ? {
		left: Math.min(zoomSelection.startX, zoomSelection.endX),
		top: Math.min(zoomSelection.startY, zoomSelection.endY),
		width: Math.abs(zoomSelection.endX - zoomSelection.startX),
		height: Math.abs(zoomSelection.endY - zoomSelection.startY)
	} : null);
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
		partname?: string;
		partdisabled?: boolean;
		laserindex?: number;
		lineIndex?: number;
	};
	let propertiesMode = $state(false);
	let hoverSegment = $state<SegmentProperties | null>(null);
	let hoverScreen = $state<{ x: number; y: number }>({ x: 0, y: 0 });

	// "Names": outlines every part of the current layer with its name. The part
	// given by the per-session platform property highlightpartuuid is always
	// outlined and emphasized, even while the toggle is off.
	type PartOverlay = {
		key: string;
		label: string;
		left: number;
		top: number;
		width: number;
		height: number;
		labelY: number;
		markers: { x: number; y: number }[];
		highlighted: boolean;
		disabled: boolean;
	};
	let namesMode = $state(false);
	let viewVersion = $state(0);
	let transformChangeCount = 0;
	let highlightPartUUID = $derived.by(() => {
		poll.v;
		const uuid = typeof platform?.highlightpartuuid === 'string' ? platform.highlightpartuuid.toLowerCase() : '';
		return uuid === NULL_UUID ? '' : uuid;
	});
	let partOverlays = $derived.by((): PartOverlay[] => {
		viewVersion;
		if (!layerViewer || (!namesMode && !highlightPartUUID)) return [];

		const overlays: PartOverlay[] = [];
		for (const box of layerViewer.getPartBoundingBoxes()) {
			const highlighted = highlightPartUUID !== '' && box.uuid === highlightPartUUID;
			if (!namesMode && !highlighted) continue;

			const topLeft = layerViewer.machineToScreen(box.minx, box.maxy);
			const bottomRight = layerViewer.machineToScreen(box.maxx, box.miny);
			if (!topLeft || !bottomRight) continue;

			const left = topLeft.x;
			const top = topLeft.y;
			const right = bottomRight.x;
			const bottom = bottomRight.y;
			const name = box.name || box.uuid.slice(0, 8);
			overlays.push({
				key: box.key,
				label: box.disabled ? `${name} (disabled)` : name,
				left,
				top,
				width: Math.max(right - left, 1),
				height: Math.max(bottom - top, 1),
				// Keep the label readable when the box touches the top edge.
				labelY: top > 14 ? top - 5 : top + 12,
				markers: [
					{ x: left, y: top },
					{ x: right, y: top },
					{ x: left, y: bottom },
					{ x: right, y: bottom },
					{ x: (left + right) / 2, y: (top + bottom) / 2 }
				],
				highlighted,
				disabled: box.disabled === true
			});
		}
		// Draw the highlighted part last so it stays on top of overlapping boxes.
		return overlays.sort((a, b) => Number(a.highlighted) - Number(b.highlighted));
	});

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

	function clearHover() {
		if (hoverFrame) {
			cancelAnimationFrame(hoverFrame);
			hoverFrame = 0;
		}
		hoverSegment = null;
		if (layerViewer && typeof layerViewer.clearHighlight === 'function') {
			layerViewer.clearHighlight();
		}
	}

	function togglePropertiesMode() {
		propertiesMode = !propertiesMode;
		hideHoverInfo();
		if (!propertiesMode) clearHover();
	}

	function toggleZoomSelectMode() {
		zoomSelectMode = !zoomSelectMode;
		zoomSelection = null;
		if (zoomSelectMode) {
			hideHoverInfo();
			clearHover();
		}
	}

	function localViewportPoint(event: PointerEvent): { x: number; y: number } | null {
		if (!containerEl) return null;
		const box = containerEl.getBoundingClientRect();
		// Pointer capture keeps delivering events outside the canvas, so clamp to its bounds.
		return {
			x: Math.min(Math.max(event.clientX - box.left, 0), box.width),
			y: Math.min(Math.max(event.clientY - box.top, 0), box.height)
		};
	}

	function finishZoomSelection() {
		const rect = zoomRect;
		zoomSelection = null;
		if (!rect || !layerViewer) return;
		if (rect.width < MIN_ZOOM_SELECTION_PX || rect.height < MIN_ZOOM_SELECTION_PX) return;

		const corner1 = layerViewer.screenToMachine(rect.left, rect.top);
		const corner2 = layerViewer.screenToMachine(rect.left + rect.width, rect.top + rect.height);
		if (!corner1 || !corner2) return;

		autoFrame = false;
		layerViewer.CenterOnRectangle(
			Math.min(corner1.x, corner2.x), Math.min(corner1.y, corner2.y),
			Math.max(corner1.x, corner2.x), Math.max(corner1.y, corner2.y)
		);
		layerViewer.RenderScene(true);
		zoomSelectMode = false;
	}

	function onWindowKeyDown(event: KeyboardEvent) {
		if (event.key === 'Escape' && zoomSelectMode) {
			zoomSelectMode = false;
			zoomSelection = null;
		}
	}

	function onPointerDown(event: PointerEvent) {
		if (zoomSelectMode && event.button === 0) {
			const point = localViewportPoint(event);
			if (!point) return;
			hideHoverInfo();
			clearHover();
			zoomSelection = { startX: point.x, startY: point.y, endX: point.x, endY: point.y };
			(event.target as HTMLElement).setPointerCapture(event.pointerId);
			return;
		}

		if (event.button === 0 || event.button === 1) {
			dragging = true;
			hideHoverInfo();
			clearHover();
			dragX = event.clientX;
			dragY = event.clientY;
			(event.target as HTMLElement).setPointerCapture(event.pointerId);
		}
	}

	function onPointerMove(event: PointerEvent) {
		updateMousePosition(event);

		if (zoomSelection) {
			const point = localViewportPoint(event);
			if (point) zoomSelection = { ...zoomSelection, endX: point.x, endY: point.y };
			return;
		}

		if (dragging && layerViewer) {
			const dx = event.clientX - dragX;
			const dy = event.clientY - dragY;
			dragX = event.clientX;
			dragY = event.clientY;
			if (dx !== 0 || dy !== 0) autoFrame = false;
			layerViewer.Drag(dx, dy);
			layerViewer.RenderScene(true);
			return;
		}

		if (!zoomSelectMode) scheduleHoverUpdate(event);
	}

	function onPointerUp() {
		if (zoomSelection) {
			finishZoomSelection();
			return;
		}
		dragging = false;
	}

	function onPointerCancel() {
		zoomSelection = null;
		dragging = false;
	}

	function onPointerLeave() {
		mousePosition = null;
		hideHoverInfo();
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
		autoFrame = true;
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
				autoFrame = false;
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
			if (autoFrame) centerOnPlatform();
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
		hideHoverInfo();
		clearHover();
		if (platform) {
			platform.displayed_layer = 0;
			platform.displayed_build = 0;
		}
	});
</script>

<svelte:window onkeydown={onWindowKeyDown} />

{#snippet layerViewBody()}
	<div class="layerview-container">
		<!-- WebGL render target — setupDOMElement sets position:relative on this -->
		<div
			bind:this={containerEl}
			class="layerview-canvas"
			class:zoom-select={zoomSelectMode}
			role="img"
			onwheel={onWheel}
			onpointerdown={onPointerDown}
			onpointermove={onPointerMove}
			onpointerup={onPointerUp}
			onpointercancel={onPointerCancel}
			onpointerleave={onPointerLeave}
		></div>

		{#if partOverlays.length > 0}
			<svg class="layerview-part-overlay" aria-hidden="true">
				{#each partOverlays as part (part.key)}
					<g
						class="layerview-part"
						class:highlighted={part.highlighted}
						class:dimmed={highlightPartUUID !== '' && !part.highlighted}
						class:disabled={part.disabled}
					>
						<rect class="layerview-part-box" x={part.left} y={part.top} width={part.width} height={part.height} />
						{#each part.markers as marker, markerIndex (markerIndex)}
							<rect
								class="layerview-part-marker"
								x={marker.x - PART_MARKER_PX / 2}
								y={marker.y - PART_MARKER_PX / 2}
								width={PART_MARKER_PX}
								height={PART_MARKER_PX}
							/>
						{/each}
						<text class="layerview-part-label" x={part.left} y={part.labelY}>{part.label}</text>
					</g>
				{/each}
			</svg>
		{/if}

		{#if zoomRect}
			<div
				class="layerview-zoom-selection"
				style={`left: ${zoomRect.left}px; top: ${zoomRect.top}px; width: ${zoomRect.width}px; height: ${zoomRect.height}px;`}
			></div>
		{/if}

		<!-- Overlaid toolbar -->
		<div class="layerview-toolbar">
			<button class="layerview-btn" onclick={resetView} title="Frame the build platform" aria-label="Zoom to platform">
				<Square size={16} />
				<span>Zoom to Platform</span>
			</button>
			<button class="layerview-btn" onclick={fitToPath} title="Frame the parts" aria-label="Zoom to parts">
				<Shapes size={16} />
				<span>Zoom to Parts</span>
			</button>
			<button
				class="layerview-btn"
				onclick={toggleZoomSelectMode}
				title="Drag a rectangle to zoom into it (Esc to cancel)"
				aria-label="Custom zoom: select a rectangle"
				aria-pressed={zoomSelectMode}
			>
				<ZoomIn size={16} />
				<span>Custom Zoom</span>
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
				onclick={() => namesMode = !namesMode}
				title="Show part outlines and names"
				aria-label="Toggle part outlines and names"
				aria-pressed={namesMode}
			>
				<Tags size={16} />
				<span>Names</span>
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
			<button
				class="layerview-btn"
				onclick={toggleToolpath}
				title="Show or hide the toolpath"
				aria-label="Toggle toolpath visibility"
				aria-pressed={toolpathVisible}
			>
				<Route size={16} />
				<span>Toolpath</span>
			</button>
			{#if pointsAvailable}
				<button
					class="layerview-btn"
					onclick={cycleColorMode}
					title={'Point color mode: ' + (COLOR_MODE_CAPTIONS[colorMode] || 'Uniform')}
					aria-label="Cycle the point color mode"
				>
					<Palette size={16} />
					<span>{COLOR_MODE_CAPTIONS[colorMode] || 'Uniform'}</span>
				</button>
				<button
					class="layerview-btn"
					onclick={toggleLaserOffPoints}
					title="Show LaserOff points"
					aria-label="Toggle LaserOff points"
					aria-pressed={showLaserOffPoints}
				>
					{#if showLaserOffPoints}<SquareCheck size={16} />{:else}<Square size={16} />{/if}
					<span>LaserOff</span>
				</button>
			{/if}
		</div>

		<!-- Layer info overlay -->
		{#if layerCount > 0}
			{#if layerJumpOpen}
				<div class="layerview-layer-info layerview-layer-jump">
					<label for="layerjump-{module.uuid}">Layer</label>
					<input
						id="layerjump-{module.uuid}"
						type="number"
						inputmode="numeric"
						min="0"
						max={layerCount - 1}
						step="1"
						bind:value={layerJumpValue}
						onkeydown={onLayerJumpKeyDown}
						onblur={() => (layerJumpOpen = false)}
						{@attach focusAndSelect}
					/>
					<span>/ {layerCount}</span>
				</div>
			{:else}
				<button
					type="button"
					class="layerview-layer-info layerview-layer-info-button"
					onclick={openLayerJump}
					title={sliderFixed ? 'Layer' : 'Click to jump to a layer'}
					aria-label={`Layer ${sliderValue} of ${layerCount}. Click to jump to a layer`}
				>
					Layer {sliderValue} / {layerCount}
				</button>
			{/if}
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

		<!-- Platform label and loading indicator -->
		{#if (labelVisible && (labelCaption || labelIcon)) || loadingLayer || loadingPoints}
			<div class={['layerview-status', { 'beside-axes': coordinateSystemVisible }]}>
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

		<!-- Live cursor position readout (machine coordinates, mm) -->
		{#if mousePosition}
			<div class="layerview-mouse-pos">
				X: {mousePosition.x.toFixed(2)} &middot; Y: {mousePosition.y.toFixed(2)} mm
			</div>
		{/if}

		<!-- Info about the point or toolpath segment under the mouse -->
		{#if hoverInfo.visible && !propertiesMode}
			<div
				class="layerview-hover-info"
				style="left: {hoverInfo.x}px; top: {hoverInfo.y}px; transform: translate({hoverInfo.flipX ? 'calc(-100% - 12px)' : '12px'}, {hoverInfo.flipY ? 'calc(-100% - 12px)' : '12px'});"
			>{hoverInfo.text}</div>
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
					{#if hoverSegment.partname}
						<dt>Part</dt>
						<dd>{hoverSegment.partname}{hoverSegment.partdisabled ? ' (disabled)' : ''}</dd>
					{/if}
					{#if hoverSegment.partid !== undefined && hoverSegment.partid !== null}
						<dt>Part ID</dt>
						<dd>{hoverSegment.partid}</dd>
					{/if}
				</dl>
			</div>
		{/if}

		<!-- Layer slider (vertical) with -/+ step buttons -->
		{#if layerCount > 0}
			<div class="layerview-slider-wrap">
				{#if !sliderFixed}
					<button class="layerview-btn layerview-step" onclick={() => changeLayerTo(sliderValue + 1)} disabled={sliderValue >= layerCount - 1} title="Next layer" aria-label="Next layer">
						<Plus size={14} />
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
					aria-label="Layer"
					aria-orientation="vertical"
				/>
				{#if !sliderFixed}
					<button class="layerview-btn layerview-step" onclick={() => changeLayerTo(sliderValue - 1)} disabled={sliderValue <= 0} title="Previous layer" aria-label="Previous layer">
						<Minus size={14} />
					</button>
				{/if}
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
	.layerview-canvas.zoom-select {
		cursor: zoom-in;
	}
	.layerview-zoom-selection {
		position: absolute;
		border: 1px dashed var(--primary, #2563eb);
		background: color-mix(in srgb, var(--primary, #2563eb) 15%, transparent);
		pointer-events: none;
		z-index: 9;
	}
	.layerview-part-overlay {
		position: absolute;
		inset: 0;
		width: 100%;
		height: 100%;
		overflow: hidden;
		pointer-events: none;
		z-index: 8;
	}
	.layerview-part-box {
		fill: none;
		stroke: var(--foreground, #333333);
		stroke-width: 1;
		stroke-dasharray: 4 3;
	}
	.layerview-part-marker {
		fill: #ef4444;
	}
	.layerview-part-label {
		font-size: 10px;
		font-weight: 600;
		fill: var(--foreground, #333333);
		/* Halo keeps the label legible on top of dense hatching. */
		paint-order: stroke;
		stroke: var(--background, #ffffff);
		stroke-width: 3px;
		stroke-linejoin: round;
	}
	.layerview-part.highlighted .layerview-part-box {
		stroke: var(--primary, #2563eb);
		stroke-width: 2;
		stroke-dasharray: none;
		fill: color-mix(in srgb, var(--primary, #2563eb) 12%, transparent);
	}
	.layerview-part.highlighted .layerview-part-marker {
		fill: var(--primary, #2563eb);
	}
	.layerview-part.highlighted .layerview-part-label {
		fill: var(--primary, #2563eb);
		font-size: 11px;
	}
	.layerview-part.dimmed {
		opacity: 0.45;
	}
	.layerview-part.disabled .layerview-part-box {
		stroke: var(--muted-foreground, #888888);
	}
	.layerview-part.disabled .layerview-part-marker {
		fill: var(--muted-foreground, #888888);
	}
	.layerview-part.disabled .layerview-part-label {
		fill: var(--destructive, #dc2626);
		text-decoration: line-through;
	}
	.layerview-toolbar {
		position: absolute;
		top: 8px;
		left: 8px;
		/* Leave room for the layer-info badge in the top-right corner */
		max-width: calc(100% - 140px);
		display: flex;
		flex-wrap: wrap;
		gap: 4px;
		z-index: 10;
	}
	.layerview-btn {
		display: inline-flex;
		flex-direction: column;
		align-items: center;
		justify-content: center;
		gap: 3px;
		width: 64px;
		height: 56px;
		padding: 6px 4px;
		border: none;
		border-radius: 4px;
		background: rgba(0, 0, 0, 0.65);
		color: white;
		font-size: 11px;
		cursor: pointer;
		transition: background-color 0.2s;
	}
	/* Two-word captions such as "Zoom to Platform" wrap onto two centered lines. */
	.layerview-btn span {
		line-height: 1.15;
		text-align: center;
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
		width: 24px;
		height: 24px;
		padding: 0;
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
	.layerview-layer-info-button {
		border: none;
		cursor: pointer;
	}
	.layerview-layer-info-button:hover {
		background: rgba(0, 0, 0, 0.85);
	}
	.layerview-layer-jump {
		display: flex;
		align-items: center;
		gap: 6px;
		padding: 2px 6px 2px 10px;
	}
	.layerview-layer-jump input {
		width: 64px;
		padding: 1px 4px;
		border: 1px solid rgba(255, 255, 255, 0.5);
		border-radius: 3px;
		background: rgba(255, 255, 255, 0.95);
		color: #111111;
		font-size: 11px;
		font-variant-numeric: tabular-nums;
		text-align: right;
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
	.layerview-status {
		position: absolute;
		left: 8px;
		bottom: 8px;
		display: flex;
		flex-wrap: wrap;
		gap: 4px;
		z-index: 10;
		pointer-events: none;
	}
	.layerview-status.beside-axes {
		/* Clear the coordinate axes indicator in the bottom-left corner */
		left: 80px;
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
		flex-direction: column;
		align-items: center;
		justify-content: center;
		gap: 6px;
		z-index: 10;
	}
	.layerview-slider {
		writing-mode: vertical-lr;
		direction: rtl;
		flex: 1;
		min-height: 0;
		width: 20px;
		accent-color: var(--primary, #2563eb);
	}
</style>
