<script lang="ts">
	import type { Snippet } from 'svelte';
	import { flip } from 'svelte/animate';
	import { dndzone } from 'svelte-dnd-action';
	import type { SequenceOperation, OperationTypeEntry, ValidationIssue, BindingReference } from './workflowTypes';
	import { issuesForOperation } from './workflowTypes';
	import {
		addOperation,
		removeOperation,
		reorderOperations,
		duplicateOperation,
		toggleEnabled,
		setOperationValue,
		setOperationComment,
		addItem,
		removeItem,
		setItemValue,
		selectOperation,
		setSequenceName,
		undo,
		findOperation,
		getOperationType,
		type WorkflowState,
	} from './workflowStore';
	import WorkflowStep from './WorkflowStep.svelte';
	import WorkflowPalette from './WorkflowPalette.svelte';
	import WorkflowToolbar from './WorkflowToolbar.svelte';
	import WorkflowStepEditor from './WorkflowStepEditor.svelte';

	let {
		state = $bindable(),
		catalog,
		readonly = false,
		nameEditable = false,
		issues = [],
		activeOperation = '',
		references = [],
		iconURL,
		toolbar,
		onchange,
	}: {
		state: WorkflowState;
		catalog: OperationTypeEntry[];
		readonly?: boolean;
		nameEditable?: boolean;
		issues?: ValidationIssue[];
		activeOperation?: string;
		references?: BindingReference[];
		iconURL?: (icon: string) => string;
		toolbar?: Snippet;
		onchange?: () => void;
	} = $props();

	const flipDurationMs = 200;

	type DndItem = { id: string; op: SequenceOperation };

	// Drag and drop works on wrappers that use the operation UUID as id. Writable while a drag is in progress.
	let dndItems: DndItem[] = $derived(state.sequence.operations.map((op) => ({ id: op.uuid, op })));

	let selectedOperation = $derived(state.selectedUUID ? findOperation(state, state.selectedUUID) : null);
	let selectedType = $derived(selectedOperation ? getOperationType(catalog, selectedOperation.type) : undefined);

	function changed(action: () => void) {
		if (readonly) return;
		action();
		onchange?.();
	}

	function handleConsider(e: CustomEvent<{ items: DndItem[] }>) {
		dndItems = e.detail.items;
	}

	function handleFinalize(e: CustomEvent<{ items: DndItem[] }>) {
		dndItems = e.detail.items;
		changed(() => reorderOperations(state, e.detail.items.map((item) => item.id)));
	}

	function handleSelect(uuid: string) {
		selectOperation(state, state.selectedUUID === uuid ? null : uuid);
	}
</script>

<div class="flex flex-col gap-2 h-full">
	<WorkflowToolbar
		canUndo={state.undoStack.length > 0}
		workflowName={state.sequence.name}
		{nameEditable}
		{readonly}
		onundo={() => changed(() => undo(state))}
		onnamechange={(name) => changed(() => setSequenceName(state, name))}
	>
		{@render toolbar?.()}
	</WorkflowToolbar>

	<div class="flex-1 flex gap-2 min-h-0">
		<div class="w-48 shrink-0 rounded-md border bg-card/30 overflow-hidden">
			<WorkflowPalette
				{catalog}
				context={state.sequence.context}
				disabled={readonly}
				{iconURL}
				onadd={(typeName) => changed(() => addOperation(state, typeName))}
			/>
		</div>

		<div class="flex-1 relative overflow-auto rounded-md border bg-card/20 p-4">
			<div
				class="flex flex-col gap-2 min-h-[120px]"
				use:dndzone={{ items: dndItems, flipDurationMs, type: 'workflow-operations', dragDisabled: readonly }}
				onconsider={handleConsider}
				onfinalize={handleFinalize}
			>
				{#each dndItems as item, index (item.id)}
					<div animate:flip={{ duration: flipDurationMs }}>
						<WorkflowStep
							operation={item.op}
							{index}
							typeEntry={getOperationType(catalog, item.op.type)}
							issues={issuesForOperation(issues, item.op.uuid)}
							selected={state.selectedUUID === item.op.uuid}
							running={activeOperation !== '' && activeOperation === item.op.uuid}
							{readonly}
							onselect={handleSelect}
							onremove={(uuid) => changed(() => removeOperation(state, uuid))}
							onduplicate={(uuid) => changed(() => duplicateOperation(state, uuid))}
							ontoggle={(uuid) => changed(() => toggleEnabled(state, uuid))}
						/>
					</div>
				{/each}
			</div>

			{#if dndItems.length === 0}
				<div class="absolute inset-0 flex items-center justify-center pointer-events-none text-muted-foreground/30 text-sm">
					{readonly ? 'This sequence has no operations' : 'Click an operation in the palette to add it'}
				</div>
			{/if}
		</div>

		{#if selectedOperation}
			<div class="w-72 shrink-0 rounded-md border bg-card/30 overflow-hidden">
				<WorkflowStepEditor
					operation={selectedOperation}
					typeEntry={selectedType}
					issues={issuesForOperation(issues, selectedOperation.uuid)}
					{references}
					{readonly}
					onvaluechange={(uuid, name, value) => changed(() => setOperationValue(state, uuid, name, value))}
					oncommentchange={(uuid, comment) => changed(() => setOperationComment(state, uuid, comment))}
					ontoggle={(uuid) => changed(() => toggleEnabled(state, uuid))}
					onadditem={(uuid, listName) => changed(() => addItem(state, catalog, uuid, listName))}
					onremoveitem={(uuid, listName, index) => changed(() => removeItem(state, uuid, listName, index))}
					onitemvaluechange={(uuid, listName, index, name, value) =>
						changed(() => setItemValue(state, uuid, listName, index, name, value))}
					onclose={() => selectOperation(state, null)}
				/>
			</div>
		{/if}
	</div>
</div>
