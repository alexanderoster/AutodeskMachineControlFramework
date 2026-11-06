import type { OperationTypeEntry } from './workflowTypes';

export interface WorkflowColors {
	stripe: string;
	tint: string;
	text: string;
}

// Surfaces stay neutral; the hue only marks the stripe, the icon tile and the category label,
// so text keeps the theme's foreground contrast in light and dark mode.
const colorMap: Record<string, WorkflowColors> = {
	teal: { stripe: 'border-l-teal-500', tint: 'bg-teal-500/15', text: 'text-teal-700 dark:text-teal-300' },
	blue: { stripe: 'border-l-sky-500', tint: 'bg-sky-500/15', text: 'text-sky-700 dark:text-sky-300' },
	violet: { stripe: 'border-l-violet-500', tint: 'bg-violet-500/15', text: 'text-violet-700 dark:text-violet-300' },
	amber: { stripe: 'border-l-amber-500', tint: 'bg-amber-500/15', text: 'text-amber-700 dark:text-amber-300' },
	pink: { stripe: 'border-l-pink-500', tint: 'bg-pink-500/15', text: 'text-pink-700 dark:text-pink-300' },
	green: { stripe: 'border-l-emerald-500', tint: 'bg-emerald-500/15', text: 'text-emerald-700 dark:text-emerald-300' },
	slate: { stripe: 'border-l-slate-400', tint: 'bg-slate-500/15', text: 'text-slate-600 dark:text-slate-300' },
	red: { stripe: 'border-l-red-500', tint: 'bg-red-500/15', text: 'text-red-700 dark:text-red-300' },
};

const categoryPalette = ['teal', 'blue', 'violet', 'amber', 'pink', 'green'];

export const OTHER_CATEGORY = 'Other';

// Sorted, unique categories of a catalog. The position in this list selects the category color.
export function categoriesOf(catalog: OperationTypeEntry[]): string[] {
	return [...new Set(catalog.map((entry) => entry.category || OTHER_CATEGORY))].sort((a, b) => a.localeCompare(b));
}

export function colorsForCategory(category: string, categories: string[]): WorkflowColors {
	const index = categories.indexOf(category || OTHER_CATEGORY);
	if (index < 0) return colorMap.slate;
	return colorMap[categoryPalette[index % categoryPalette.length]];
}

// Types with an explicit color use it; all others get the color of their category. Unknown types are red.
export function colorsFor(entry: OperationTypeEntry | undefined, categories: string[]): WorkflowColors {
	if (!entry) return colorMap.red;
	if (entry.color && colorMap[entry.color]) return colorMap[entry.color];
	return colorsForCategory(entry.category, categories);
}
