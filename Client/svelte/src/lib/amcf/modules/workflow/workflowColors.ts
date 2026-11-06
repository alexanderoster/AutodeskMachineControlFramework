import type { OperationTypeEntry } from './workflowTypes';

export interface WorkflowColors {
	bg: string;
	border: string;
	text: string;
	badge: string;
}

const colorMap: Record<string, WorkflowColors> = {
	blue: { bg: 'bg-blue-500/10', border: 'border-blue-500/40', text: 'text-blue-200', badge: 'bg-blue-600 text-white' },
	amber: { bg: 'bg-amber-500/10', border: 'border-amber-500/40', text: 'text-amber-200', badge: 'bg-amber-600 text-white' },
	green: { bg: 'bg-green-500/10', border: 'border-green-500/40', text: 'text-green-200', badge: 'bg-green-600 text-white' },
	red: { bg: 'bg-red-500/10', border: 'border-red-500/40', text: 'text-red-200', badge: 'bg-red-600 text-white' },
	violet: { bg: 'bg-violet-500/10', border: 'border-violet-500/40', text: 'text-violet-200', badge: 'bg-violet-600 text-white' },
	cyan: { bg: 'bg-cyan-500/10', border: 'border-cyan-500/40', text: 'text-cyan-200', badge: 'bg-cyan-600 text-white' },
};

const categoryPalette = ['blue', 'amber', 'green', 'violet', 'cyan'];

// Types without an explicit color get a stable color per category.
export function colorsFor(entry: OperationTypeEntry | undefined): WorkflowColors {
	if (!entry) return colorMap.red;
	if (entry.color && colorMap[entry.color]) return colorMap[entry.color];

	let hash = 0;
	for (const c of entry.category || '') hash = (hash * 31 + c.charCodeAt(0)) >>> 0;
	return colorMap[categoryPalette[hash % categoryPalette.length]];
}
