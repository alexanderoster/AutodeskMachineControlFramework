export type OperationParameterType = 'string' | 'integer' | 'double' | 'bool' | 'enum';

export type OperationExpressionMode = 'literal' | 'reference' | 'formula';

export interface OperationEnumOption {
	value: string;
	displayname: string;
}

export interface OperationParameterEntry {
	name: string;
	displayname: string;
	description?: string;
	type: OperationParameterType;
	unit?: string;
	default: string;
	min?: number;
	max?: number;
	expression: OperationExpressionMode;
	required: boolean;
	options?: OperationEnumOption[];
}

export interface OperationItemListEntry {
	name: string;
	displayname: string;
	maxcount: number;
	parameters: OperationParameterEntry[];
}

export interface OperationTypeEntry {
	type: string;
	displayname: string;
	description?: string;
	category: string;
	icon?: string;
	color?: string;
	contexts: string[];
	parameters: OperationParameterEntry[];
	itemlists: OperationItemListEntry[];
}

// Values stay strings, because they can be references or formulas.
export type OperationValues = Record<string, string>;

export interface SequenceOperation {
	uuid: string;
	type: string;
	enabled: boolean;
	comment: string;
	values: OperationValues;
	items: Record<string, OperationValues[]>;
}

export interface OperationSequenceData {
	name: string;
	displayname: string;
	description: string;
	context: string;
	bindinggroup: string;
	revision: number;
	published?: boolean;
	uuid?: string;
	timestamp?: string;
	user?: string;
	origin?: string;
	operations: SequenceOperation[];
}

export interface OperationSequenceSummary {
	name: string;
	displayname: string;
	context: string;
	revision: number;
	uuid: string;
	timestamp: string;
	user: string;
	username: string;
	origin: string;
	hasdefault: boolean;
}

export interface OperationSequenceRevisionEntry {
	revision: number;
	uuid: string;
	timestamp: string;
	user: string;
	username: string;
	origin: string;
}

export interface BindingReference {
	name: string;
	description: string;
}

export interface ValidationIssue {
	severity: 'error' | 'warning';
	operation: string;
	parameter: string;
	message: string;
}

export interface ValidationResult {
	valid: boolean;
	issues: ValidationIssue[];
}

export function issuesForOperation(issues: ValidationIssue[], operationUUID: string): ValidationIssue[] {
	return issues.filter((issue) => issue.operation === operationUUID);
}

export function isReferenceValue(value: string): boolean {
	return value.trim().startsWith('$');
}

export function isNumericParameter(parameter: OperationParameterEntry): boolean {
	return parameter.type === 'integer' || parameter.type === 'double';
}
