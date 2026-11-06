<script lang="ts">
	import { untrack } from 'svelte';
	import { useModuleTick } from '$lib/amcf/poll.svelte';
	import WorkflowEditor from './workflow/WorkflowEditor.svelte';
	import type { OperationSequenceData, OperationTypeEntry } from './workflow/workflowTypes';
	import { createWorkflowState } from './workflow/workflowStore';

	let { module, app }: { module: any; app: any } = $props();
	const poll = useModuleTick(() => module);

	let visible = $derived.by(() => {
		poll.v;
		return module.visible !== false;
	});

	// The stub has no server round trip; the editor owns a local copy of the example workflow.
	let workflowState = $state(
		createWorkflowState(untrack((): OperationSequenceData => module.workflowData || module.getDefaultWorkflow())),
	);

	let catalog: OperationTypeEntry[] = $derived.by(() => {
		poll.v;
		return module.catalogData || module.getDefaultCatalog();
	});
</script>

{#if visible}
	<div class="w-full" style="min-height: 500px; height: 70vh;">
		<WorkflowEditor bind:state={workflowState} {catalog} nameEditable />
	</div>
{/if}
