<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import { Button } from '$lib/components/ui/button/index.js';
	import RotateCcw from '@lucide/svelte/icons/rotate-ccw';
	import Maximize from '@lucide/svelte/icons/maximize';

	import { usePollTick } from '$lib/amcf/poll.svelte';

	let { module, app }: { module: any; app: any } = $props();
	const poll = usePollTick();

	let visible = $derived.by(() => { poll.v; return module.visible !== false; });
	let containerEl: HTMLDivElement | undefined = $state(undefined);
	let glInstance: any = $state(null);
	let layerViewerInstance: any = $state(null);

	let platform = $derived(module.platform || null);
	let currentLayer = $derived(platform?.currentlayer || 0);
	let layerCount = $derived(platform?.layercount || 0);
	let sliderValue = $state(0);

	$effect(() => {
		if (platform) {
			sliderValue = platform.currentlayer || 0;
		}
	});

	function onSliderChange (e: Event) {
		const val = parseInt((e.target as HTMLInputElement).value);
		sliderValue = val;
		if (platform && layerViewerInstance) {
			platform.currentlayer = val;
			try { layerViewerInstance.updateLayer(val); } catch {}
		}
	}

	onMount(() => {
		if (!containerEl || !app) return;

		try {
			glInstance = app.retrieveWebGLInstance(module.uuid);
			if (glInstance && containerEl) {
				glInstance.setupDOMElement(containerEl);
				glInstance.setupOrthographicView();
			}
		} catch (e) {
			console.warn('[LayerView] WebGL init skipped:', e);
		}

		try {
			const LayerViewerClass = (window as any).AMCImplementation_LayerView;
			if (LayerViewerClass && glInstance) {
				layerViewerInstance = new LayerViewerClass(glInstance, app, module);
			}
		} catch {}

		module.onDataHasChanged = () => {
			try { layerViewerInstance?.updateLayer(platform?.currentlayer || 0); } catch {}
		};

		const ro = new ResizeObserver(() => {
			try { glInstance?.resize(); } catch {}
		});
		ro.observe(containerEl);

		return () => ro.disconnect();
	});

	onDestroy(() => {
		module.onDataHasChanged = null;
	});

	function resetView () {
		try { glInstance?.resetView(); } catch {}
	}

	function fitToPath () {
		try { layerViewerInstance?.fitToPath(); } catch {}
	}
</script>

{#if visible}
	<div class="w-full flex flex-col gap-2">
		<!-- Toolbar -->
		<div class="flex items-center gap-2">
			<Button variant="outline" size="sm" class="h-7 text-xs" onclick={resetView}>
				<RotateCcw class="h-3.5 w-3.5 mr-1" /> Reset View
			</Button>
			<Button variant="outline" size="sm" class="h-7 text-xs" onclick={fitToPath}>
				<Maximize class="h-3.5 w-3.5 mr-1" /> Fit
			</Button>
			<div class="flex-1"></div>
			{#if layerCount > 0}
				<span class="text-xs text-muted-foreground tabular-nums">
					Layer {sliderValue} / {layerCount}
				</span>
			{/if}
		</div>

		<div class="relative w-full border rounded-md overflow-hidden" style="min-height: 400px;">
			<div bind:this={containerEl} class="w-full h-full absolute inset-0"></div>
			{#if !glInstance}
				<div class="absolute inset-0 flex items-center justify-center text-sm text-muted-foreground bg-muted">
					Layer view requires WebGL.
				</div>
			{/if}
		</div>

		<!-- Layer slider -->
		{#if layerCount > 0}
			<input
				type="range"
				class="w-full accent-primary"
				min="0"
				max={layerCount}
				value={sliderValue}
				oninput={onSliderChange}
			/>
		{/if}
	</div>
{/if}
