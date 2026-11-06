<script lang="ts">
	import type { OperationTypeEntry } from './workflowTypes';
	import { Separator } from '$lib/components/ui/separator';
	import { colorsFor, colorsForCategory, OTHER_CATEGORY } from './workflowColors';

	let {
		catalog,
		categories = [],
		context = '',
		disabled = false,
		iconURL,
		onadd,
	}: {
		catalog: OperationTypeEntry[];
		categories?: string[];
		context?: string;
		disabled?: boolean;
		iconURL?: (icon: string) => string;
		onadd?: (typeName: string) => void;
	} = $props();

	let groups = $derived.by(() => {
		const byCategory = new Map<string, OperationTypeEntry[]>();
		for (const entry of catalog) {
			if (entry.contexts.length > 0 && !entry.contexts.includes(context)) continue;
			const category = entry.category || OTHER_CATEGORY;
			const list = byCategory.get(category) ?? [];
			list.push(entry);
			byCategory.set(category, list);
		}
		return [...byCategory.entries()]
			.sort(([a], [b]) => a.localeCompare(b))
			.map(([category, entries]) => ({
				category,
				entries: [...entries].sort((a, b) => (a.displayname || a.type).localeCompare(b.displayname || b.type)),
			}));
	});
</script>

<div class="flex flex-col h-full">
	<h3 class="text-xs font-semibold uppercase tracking-wider text-muted-foreground px-3 py-2">
		Operations
	</h3>
	<div class="flex-1 min-h-0 overflow-y-auto">
			<div class="flex flex-col gap-1 px-2 pb-2">
				{#each groups as group, groupIndex (group.category)}
					{#if groupIndex > 0}
						<Separator class="my-1" />
					{/if}
					<span class={['text-[10px] uppercase tracking-wider font-semibold px-1 pt-1', colorsForCategory(group.category, categories).text]}>
						{group.category}
					</span>
					{#each group.entries as entry (entry.type)}
						{@const colors = colorsFor(entry, categories)}
						{@const icon = entry.icon && iconURL ? iconURL(entry.icon) : ''}
						<button
							class={[
								'w-full flex items-center gap-2 rounded border border-l-4 bg-card px-2 py-1.5 text-left text-sm transition-colors',
								colors.stripe,
								disabled ? 'cursor-default' : 'hover:bg-accent hover:shadow-sm',
							]}
							title={entry.description || entry.type}
							{disabled}
							onclick={() => onadd?.(entry.type)}
						>
							{#if icon}
								<img src={icon} alt="" class="h-4 w-4 shrink-0 object-contain" />
							{/if}
							<span class="text-xs font-medium text-foreground truncate">{entry.displayname || entry.type}</span>
						</button>
					{/each}
				{:else}
					<p class="text-xs text-muted-foreground italic px-1 py-2">No operation types available</p>
				{/each}
			</div>
	</div>
</div>
