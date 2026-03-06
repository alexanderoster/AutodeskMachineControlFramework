<script lang="ts">
	import { onMount, onDestroy } from 'svelte';

	import { usePollTick } from '$lib/amcf/poll.svelte';

	let { module, app }: { module: any; app: any } = $props();
	const poll = usePollTick();

	let visible = $derived.by(() => { poll.v; return module.visible !== false; });
	let containerEl: HTMLDivElement | undefined = $state(undefined);
	let glInstance: any = $state(null);
	let initialized = $state(false);

	onMount(() => {
		if (!containerEl || !app) return;

		try {
			glInstance = app.retrieveWebGLInstance(module.uuid);
			if (glInstance && containerEl) {
				glInstance.setupDOMElement(containerEl);
				glInstance.setupPerspectiveView(45, containerEl.clientWidth / containerEl.clientHeight, 1, 3000);
				glInstance.setCameraPosition(150, 150, 150);
				glInstance.setLightParameters(0.6, 0.5, 0.2);
				initialized = true;
			}
		} catch (e) {
			console.warn('[GLScene] WebGL init skipped:', e);
		}

		if (initialized && module.scene?.instances) {
			for (const inst of module.scene.instances) {
				try {
					glInstance.addMeshElement(inst.instancename, app, inst.meshuuid);
				} catch {}
			}
		}

		module.onDataHasChanged = () => {
			if (!glInstance || !module.scene?.instances) return;
			for (const inst of module.scene.instances) {
				try {
					glInstance.updateMeshElement(inst.instancename, inst);
				} catch {}
			}
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
</script>

{#if visible}
	<div class="w-full border rounded-md overflow-hidden relative" style="min-height: 400px;">
		<div bind:this={containerEl} class="w-full h-full absolute inset-0"></div>
		{#if !initialized}
			<div class="absolute inset-0 flex items-center justify-center text-sm text-muted-foreground bg-muted">
				3D scene requires WebGL.
			</div>
		{/if}
	</div>
{/if}
