<script lang="ts">
	import type { SequenceOperation, OperationTypeEntry, ValidationIssue, BindingReference } from './workflowTypes';
	import { Label } from '$lib/components/ui/label';
	import { Separator } from '$lib/components/ui/separator';
	import WorkflowParameterField from './WorkflowParameterField.svelte';

	let {
		operation,
		typeEntry,
		issues = [],
		references = [],
		readonly = false,
		onvaluechange,
		oncommentchange,
		ontoggle,
		onadditem,
		onremoveitem,
		onitemvaluechange,
		onclose,
	}: {
		operation: SequenceOperation | null;
		typeEntry: OperationTypeEntry | undefined;
		issues?: ValidationIssue[];
		references?: BindingReference[];
		readonly?: boolean;
		onvaluechange?: (uuid: string, name: string, value: string | undefined) => void;
		oncommentchange?: (uuid: string, comment: string) => void;
		ontoggle?: (uuid: string) => void;
		onadditem?: (uuid: string, listName: string) => void;
		onremoveitem?: (uuid: string, listName: string, index: number) => void;
		onitemvaluechange?: (uuid: string, listName: string, index: number, name: string, value: string | undefined) => void;
		onclose?: () => void;
	} = $props();

	function issuesFor(label: string): ValidationIssue[] {
		return issues.filter((issue) => issue.parameter === label);
	}

	// Issues that are not attached to a field of this editor, for example unknown parameters.
	let generalIssues = $derived.by(() => {
		if (!typeEntry) return issues;
		const labels = new Set<string>(typeEntry.parameters.map((p) => p.name));
		for (const list of typeEntry.itemlists) {
			const items = operation?.items[list.name] ?? [];
			items.forEach((_, index) => {
				for (const parameter of list.parameters) labels.add(`${list.name}[${index}].${parameter.name}`);
			});
		}
		return issues.filter((issue) => !labels.has(issue.parameter));
	});
</script>

{#if operation}
	<div class="flex flex-col h-full">
		<div class="flex items-center justify-between px-3 py-2">
			<h3 class="text-xs font-semibold uppercase tracking-wider text-muted-foreground/60">Properties</h3>
			<button class="text-xs text-muted-foreground/40 hover:text-foreground transition-colors" onclick={onclose}>✕</button>
		</div>
		<Separator />
		<div class="flex-1 min-h-0 overflow-y-auto">
				<div class="flex flex-col gap-3 px-3 py-3">
					<div>
						<Label class="text-[10px] uppercase text-muted-foreground/50">Operation</Label>
						<p class="text-sm font-medium text-foreground">{typeEntry?.displayname || operation.type}</p>
						{#if typeEntry?.description}
							<p class="text-xs text-muted-foreground/60">{typeEntry.description}</p>
						{/if}
						{#if !typeEntry}
							<p class="text-xs text-red-400">The type "{operation.type}" is not registered.</p>
						{/if}
					</div>

					<label class="flex items-center gap-2 text-sm">
						<input type="checkbox" checked={operation.enabled} disabled={readonly} onchange={() => ontoggle?.(operation.uuid)} />
						Enabled
					</label>

					<div class="flex flex-col gap-1">
						<Label for="comment-{operation.uuid}" class="text-xs text-muted-foreground">Comment</Label>
						<textarea
							id="comment-{operation.uuid}"
							class="min-h-14 w-full rounded border border-input bg-background/50 px-2 py-1 text-sm outline-none focus:ring-1 focus:ring-primary"
							value={operation.comment}
							disabled={readonly}
							oninput={(e) => oncommentchange?.(operation.uuid, e.currentTarget.value)}
						></textarea>
					</div>

					{#each generalIssues as issue, issueIndex (issueIndex)}
						<p class={['text-xs', issue.severity === 'error' ? 'text-red-400' : 'text-amber-400']}>{issue.message}</p>
					{/each}

					{#if typeEntry && typeEntry.parameters.length > 0}
						<Separator />
						<span class="text-[10px] uppercase tracking-wider text-muted-foreground/40">Parameters</span>
						{#each typeEntry.parameters as parameter (parameter.name)}
							{@const fieldIssues = issuesFor(parameter.name)}
							<div class="flex flex-col gap-1">
								<span class="text-xs text-muted-foreground" title={parameter.description || parameter.name}>
									{parameter.displayname || parameter.name}
									{#if parameter.required}<span class="text-red-400">*</span>{/if}
								</span>
								<WorkflowParameterField
									{parameter}
									value={operation.values[parameter.name]}
									{references}
									{readonly}
									invalid={fieldIssues.some((i) => i.severity === 'error')}
									onchange={(value) => onvaluechange?.(operation.uuid, parameter.name, value)}
								/>
								{#each fieldIssues as issue, issueIndex (issueIndex)}
									<span class={['text-[11px]', issue.severity === 'error' ? 'text-red-400' : 'text-amber-400']}>{issue.message}</span>
								{/each}
							</div>
						{/each}
					{:else if typeEntry}
						<p class="text-xs text-muted-foreground/50 italic">No configurable parameters</p>
					{/if}

					{#each typeEntry?.itemlists ?? [] as list (list.name)}
						{@const items = operation.items[list.name] ?? []}
						<Separator />
						<div class="flex items-center justify-between">
							<span class="text-[10px] uppercase tracking-wider text-muted-foreground/40">
								{list.displayname || list.name} ({items.length}/{list.maxcount})
							</span>
							{#if !readonly}
								<button
									class="text-xs text-primary disabled:opacity-40"
									disabled={items.length >= list.maxcount}
									onclick={() => onadditem?.(operation.uuid, list.name)}
								>
									+ Add
								</button>
							{/if}
						</div>
						{#if items.length > 0}
							<div class="overflow-x-auto">
								<table class="w-full text-xs">
									<thead>
										<tr>
											<th class="text-left font-normal text-muted-foreground/50 pr-1">#</th>
											{#each list.parameters as parameter (parameter.name)}
												<th class="text-left font-normal text-muted-foreground/50 pr-1" title={parameter.description || parameter.name}>
													{parameter.displayname || parameter.name}{parameter.unit ? ` (${parameter.unit})` : ''}
												</th>
											{/each}
											<th></th>
										</tr>
									</thead>
									<tbody>
										{#each items as item, itemIndex (itemIndex)}
											<tr>
												<td class="pr-1 text-muted-foreground/50 tabular-nums">{itemIndex + 1}</td>
												{#each list.parameters as parameter (parameter.name)}
													{@const cellIssues = issuesFor(`${list.name}[${itemIndex}].${parameter.name}`)}
													<td class="pr-1 py-0.5 min-w-20" title={cellIssues.map((i) => i.message).join('\n')}>
														<WorkflowParameterField
															{parameter}
															value={item[parameter.name]}
															{references}
															{readonly}
															compact
															invalid={cellIssues.some((i) => i.severity === 'error')}
															onchange={(value) => onitemvaluechange?.(operation.uuid, list.name, itemIndex, parameter.name, value)}
														/>
													</td>
												{/each}
												<td>
													{#if !readonly}
														<button
															class="text-muted-foreground/50 hover:text-destructive px-1"
															title="Remove item"
															onclick={() => onremoveitem?.(operation.uuid, list.name, itemIndex)}
														>
															✕
														</button>
													{/if}
												</td>
											</tr>
										{/each}
									</tbody>
								</table>
							</div>
						{/if}
					{/each}
				</div>
		</div>
	</div>
{:else}
	<div class="flex items-center justify-center h-full px-4">
		<p class="text-xs text-muted-foreground/40 text-center">Select an operation to view its properties</p>
	</div>
{/if}
