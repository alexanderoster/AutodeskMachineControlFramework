<script lang="ts">
	import type { Snippet } from 'svelte';
	import { Button } from '$lib/components/ui/button';
	import { Separator } from '$lib/components/ui/separator';

	let {
		canUndo = false,
		workflowName = '',
		nameEditable = false,
		readonly = false,
		onundo,
		onnamechange,
		children,
	}: {
		canUndo?: boolean;
		workflowName?: string;
		nameEditable?: boolean;
		readonly?: boolean;
		onundo?: () => void;
		onnamechange?: (name: string) => void;
		children?: Snippet;
	} = $props();
</script>

<div class="flex flex-wrap items-center gap-2 rounded-md border bg-card/50 px-3 py-2">
	{#if nameEditable}
		<input
			type="text"
			value={workflowName}
			oninput={(e) => onnamechange?.(e.currentTarget.value)}
			class="bg-transparent text-sm font-semibold text-foreground border-none outline-none w-48 placeholder:text-muted-foreground/40"
			placeholder="Workflow name..."
		/>
		<Separator orientation="vertical" class="h-5" />
	{/if}

	{@render children?.()}

	<div class="flex-1"></div>

	{#if !readonly}
		<Button variant="ghost" size="sm" disabled={!canUndo} onclick={onundo}>↩ Undo</Button>
	{/if}
</div>
