<script lang="ts">
	import type { SequenceOperation, OperationTypeEntry, ValidationIssue } from './workflowTypes';
	import { Badge } from '$lib/components/ui/badge';
	import { colorsFor } from './workflowColors';

	let {
		operation,
		index,
		typeEntry,
		issues = [],
		selected = false,
		running = false,
		readonly = false,
		onselect,
		onremove,
		onduplicate,
		ontoggle,
	}: {
		operation: SequenceOperation;
		index: number;
		typeEntry: OperationTypeEntry | undefined;
		issues?: ValidationIssue[];
		selected?: boolean;
		running?: boolean;
		readonly?: boolean;
		onselect?: (uuid: string) => void;
		onremove?: (uuid: string) => void;
		onduplicate?: (uuid: string) => void;
		ontoggle?: (uuid: string) => void;
	} = $props();

	const MAX_SUMMARY_VALUES = 4;

	let label = $derived(typeEntry?.displayname || operation.type);
	let colors = $derived(colorsFor(typeEntry));
	let errorCount = $derived(issues.filter((issue) => issue.severity === 'error').length);
	let warningCount = $derived(issues.length - errorCount);

	let summary = $derived.by(() => {
		const parts: string[] = [];
		const parameters = typeEntry?.parameters ?? [];
		for (const parameter of parameters) {
			const value = operation.values[parameter.name];
			if (value === undefined) continue;
			const option = parameter.options?.find((o) => o.value === value);
			const text = option ? option.displayname || option.value : value;
			parts.push(parameter.unit ? `${text} ${parameter.unit}` : text);
			if (parts.length >= MAX_SUMMARY_VALUES) break;
		}
		if (!typeEntry) {
			for (const [name, value] of Object.entries(operation.values)) {
				parts.push(`${name}=${value}`);
				if (parts.length >= MAX_SUMMARY_VALUES) break;
			}
		}
		const itemCounts = Object.entries(operation.items)
			.filter(([, items]) => items.length > 0)
			.map(([listName, items]) => {
				const listLabel = typeEntry?.itemlists.find((list) => list.name === listName)?.displayname || listName;
				return `${items.length} × ${listLabel}`;
			});
		return [...parts, ...itemCounts].join(' · ');
	});

	function stop(e: Event, action: (() => void) | undefined) {
		e.stopPropagation();
		action?.();
	}

	function handleKeydown(e: KeyboardEvent) {
		if (e.key === 'Enter' || e.key === ' ') {
			e.preventDefault();
			onselect?.(operation.uuid);
		}
	}
</script>

<div
	class={[
		'group flex items-start gap-2 rounded-md border px-3 py-2 transition-all',
		readonly ? 'cursor-pointer' : 'cursor-grab active:cursor-grabbing',
		colors.bg,
		colors.border,
		!operation.enabled && 'opacity-50 border-dashed',
		running && 'ring-2 ring-green-500 shadow-md',
		selected && !running && 'ring-2 ring-primary shadow-md',
		!selected && !running && 'hover:shadow-sm',
	]}
	onclick={() => onselect?.(operation.uuid)}
	onkeydown={handleKeydown}
	role="button"
	tabindex="0"
	data-operation-uuid={operation.uuid}
>
	<span class="text-muted-foreground/50 select-none tabular-nums text-xs pt-0.5 w-6 text-right" title="Position">
		{index + 1}
	</span>

	<div class="flex-1 min-w-0 flex flex-col gap-0.5">
		<div class="flex items-center gap-2">
			<span class={[colors.text, 'text-sm font-medium', !operation.enabled && 'line-through']}>{label}</span>
			{#if !typeEntry}
				<Badge class="bg-red-600 text-white text-[9px] px-1 py-0 leading-tight">unknown type</Badge>
			{/if}
			{#if running}
				<Badge class="bg-green-600 text-white text-[9px] px-1 py-0 leading-tight">running</Badge>
			{/if}
			{#if errorCount > 0}
				<Badge class="bg-red-600 text-white text-[9px] px-1 py-0 leading-tight" title={issues.map((i) => i.message).join('\n')}>
					{errorCount} {errorCount === 1 ? 'error' : 'errors'}
				</Badge>
			{:else if warningCount > 0}
				<Badge class="bg-amber-600 text-white text-[9px] px-1 py-0 leading-tight" title={issues.map((i) => i.message).join('\n')}>
					{warningCount} {warningCount === 1 ? 'warning' : 'warnings'}
				</Badge>
			{/if}
		</div>
		{#if summary}
			<span class="text-xs text-muted-foreground/70 tabular-nums truncate">{summary}</span>
		{/if}
		{#if operation.comment}
			<span class="text-xs italic text-muted-foreground/50 truncate">{operation.comment}</span>
		{/if}
	</div>

	{#if !readonly}
		<div class="flex items-center gap-0.5 opacity-0 group-hover:opacity-100 transition-opacity">
			<button
				class="text-muted-foreground/50 hover:text-foreground text-xs px-1"
				title={operation.enabled ? 'Disable operation' : 'Enable operation'}
				onclick={(e) => stop(e, () => ontoggle?.(operation.uuid))}
			>
				{operation.enabled ? '⏸' : '▶'}
			</button>
			<button
				class="text-muted-foreground/50 hover:text-foreground text-xs px-1"
				title="Duplicate operation"
				onclick={(e) => stop(e, () => onduplicate?.(operation.uuid))}
			>
				⧉
			</button>
			<button
				class="text-muted-foreground/50 hover:text-destructive text-sm px-1"
				title="Remove operation"
				onclick={(e) => stop(e, () => onremove?.(operation.uuid))}
			>
				✕
			</button>
		</div>
	{/if}
</div>
