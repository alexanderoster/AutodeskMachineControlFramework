import type {
	OperationSequenceData,
	OperationTypeEntry,
	SequenceOperation,
	OperationValues,
} from './workflowTypes';

const MAX_UNDO = 50;

export interface WorkflowState {
	sequence: OperationSequenceData;
	selectedUUID: string | null;
	undoStack: string[];
	lastUndoKey: string | null;
	dirty: boolean;
	baseRevision: number;
}

// crypto.randomUUID is only available in secure contexts; machine UIs are often served over plain HTTP.
export function createOperationUUID(): string {
	const bytes = new Uint8Array(16);
	crypto.getRandomValues(bytes);
	bytes[6] = (bytes[6] & 0x0f) | 0x40;
	bytes[8] = (bytes[8] & 0x3f) | 0x80;
	const hex = Array.from(bytes, (b) => b.toString(16).padStart(2, '0')).join('');
	return `${hex.slice(0, 8)}-${hex.slice(8, 12)}-${hex.slice(12, 16)}-${hex.slice(16, 20)}-${hex.slice(20)}`;
}

export function cloneSequence(sequence: OperationSequenceData): OperationSequenceData {
	return JSON.parse(JSON.stringify(sequence));
}

export function createWorkflowState(sequence: OperationSequenceData): WorkflowState {
	return {
		sequence: cloneSequence(sequence),
		selectedUUID: null,
		undoStack: [],
		lastUndoKey: null,
		dirty: false,
		baseRevision: sequence.revision,
	};
}

// Replaces the edited sequence, e.g. after loading or saving. Clears undo and the dirty flag.
export function resetWorkflowState(state: WorkflowState, sequence: OperationSequenceData, dirty: boolean = false): void {
	state.sequence = cloneSequence(sequence);
	state.undoStack = [];
	state.lastUndoKey = null;
	state.dirty = dirty;
	state.baseRevision = sequence.revision;
	if (state.selectedUUID && !findOperation(state, state.selectedUUID)) state.selectedUUID = null;
}

// Consecutive edits with the same key (for example typing into one field) share one undo step.
function pushUndo(state: WorkflowState, key: string | null = null): void {
	state.dirty = true;
	if (key !== null && key === state.lastUndoKey) return;
	state.lastUndoKey = key;
	state.undoStack.push(JSON.stringify(state.sequence.operations));
	if (state.undoStack.length > MAX_UNDO) state.undoStack.shift();
}

export function undo(state: WorkflowState): void {
	const previous = state.undoStack.pop();
	if (previous === undefined) return;
	state.sequence.operations = JSON.parse(previous);
	state.lastUndoKey = null;
	if (state.selectedUUID && !findOperation(state, state.selectedUUID)) state.selectedUUID = null;
}

export function findOperation(state: WorkflowState, uuid: string): SequenceOperation | null {
	return state.sequence.operations.find((op) => op.uuid === uuid) ?? null;
}

export function getOperationType(catalog: OperationTypeEntry[], typeName: string): OperationTypeEntry | undefined {
	return catalog.find((entry) => entry.type === typeName);
}

export function addOperation(state: WorkflowState, typeName: string, index?: number): SequenceOperation {
	pushUndo(state);
	const operation: SequenceOperation = {
		uuid: createOperationUUID(),
		type: typeName,
		enabled: true,
		comment: '',
		values: {},
		items: {},
	};
	const operations = state.sequence.operations;
	if (index !== undefined && index >= 0 && index <= operations.length) operations.splice(index, 0, operation);
	else operations.push(operation);
	state.selectedUUID = operation.uuid;
	return operation;
}

export function removeOperation(state: WorkflowState, uuid: string): void {
	const index = state.sequence.operations.findIndex((op) => op.uuid === uuid);
	if (index === -1) return;
	pushUndo(state);
	state.sequence.operations.splice(index, 1);
	if (state.selectedUUID === uuid) state.selectedUUID = null;
}

export function moveOperation(state: WorkflowState, fromIndex: number, toIndex: number): void {
	const operations = state.sequence.operations;
	if (fromIndex < 0 || fromIndex >= operations.length || toIndex < 0 || toIndex >= operations.length) return;
	if (fromIndex === toIndex) return;
	pushUndo(state);
	const [operation] = operations.splice(fromIndex, 1);
	operations.splice(toIndex, 0, operation);
}

// Applies a new order, given as a list of operation UUIDs (for example from drag and drop).
export function reorderOperations(state: WorkflowState, orderedUUIDs: string[]): void {
	const operations = state.sequence.operations;
	const current = operations.map((op) => op.uuid);
	if (current.length !== orderedUUIDs.length || current.every((uuid, i) => uuid === orderedUUIDs[i])) return;

	const byUUID = new Map(operations.map((op) => [op.uuid, op]));
	const reordered = orderedUUIDs.map((uuid) => byUUID.get(uuid)).filter((op): op is SequenceOperation => op !== undefined);
	if (reordered.length !== operations.length) return;

	pushUndo(state);
	state.sequence.operations = reordered;
}

export function duplicateOperation(state: WorkflowState, uuid: string): SequenceOperation | null {
	const index = state.sequence.operations.findIndex((op) => op.uuid === uuid);
	if (index === -1) return null;
	pushUndo(state);
	const copy: SequenceOperation = JSON.parse(JSON.stringify(state.sequence.operations[index]));
	copy.uuid = createOperationUUID();
	state.sequence.operations.splice(index + 1, 0, copy);
	state.selectedUUID = copy.uuid;
	return copy;
}

export function toggleEnabled(state: WorkflowState, uuid: string): void {
	const operation = findOperation(state, uuid);
	if (!operation) return;
	pushUndo(state);
	operation.enabled = !operation.enabled;
}

// An undefined or empty value removes the explicit value, so that the default applies.
export function setOperationValue(state: WorkflowState, uuid: string, name: string, value: string | undefined): void {
	const operation = findOperation(state, uuid);
	if (!operation) return;
	pushUndo(state, `value:${uuid}:${name}`);
	if (value === undefined || value === '') delete operation.values[name];
	else operation.values[name] = value;
}

export function setOperationComment(state: WorkflowState, uuid: string, comment: string): void {
	const operation = findOperation(state, uuid);
	if (!operation) return;
	pushUndo(state, `comment:${uuid}`);
	operation.comment = comment;
}

export function addItem(state: WorkflowState, catalog: OperationTypeEntry[], uuid: string, listName: string): void {
	const operation = findOperation(state, uuid);
	if (!operation) return;
	const listEntry = getOperationType(catalog, operation.type)?.itemlists.find((list) => list.name === listName);
	const items = operation.items[listName] ?? [];
	if (listEntry && items.length >= listEntry.maxcount) return;
	pushUndo(state);
	operation.items[listName] = [...items, {}];
}

export function removeItem(state: WorkflowState, uuid: string, listName: string, index: number): void {
	const operation = findOperation(state, uuid);
	const items = operation?.items[listName];
	if (!operation || !items || index < 0 || index >= items.length) return;
	pushUndo(state);
	items.splice(index, 1);
	if (items.length === 0) delete operation.items[listName];
}

export function setItemValue(
	state: WorkflowState,
	uuid: string,
	listName: string,
	index: number,
	name: string,
	value: string | undefined,
): void {
	const item: OperationValues | undefined = findOperation(state, uuid)?.items[listName]?.[index];
	if (!item) return;
	pushUndo(state, `item:${uuid}:${listName}:${index}:${name}`);
	if (value === undefined || value === '') delete item[name];
	else item[name] = value;
}

export function selectOperation(state: WorkflowState, uuid: string | null): void {
	state.selectedUUID = uuid;
}

export function setSequenceName(state: WorkflowState, name: string): void {
	state.sequence.name = name;
	state.dirty = true;
}
