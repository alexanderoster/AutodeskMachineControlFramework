<script lang="ts">
	import type { OperationParameterEntry, BindingReference } from './workflowTypes';
	import { isNumericParameter, isReferenceValue } from './workflowTypes';

	let {
		parameter,
		value,
		references = [],
		readonly = false,
		compact = false,
		invalid = false,
		onchange,
	}: {
		parameter: OperationParameterEntry;
		value: string | undefined;
		references?: BindingReference[];
		readonly?: boolean;
		compact?: boolean;
		invalid?: boolean;
		onchange?: (value: string | undefined) => void;
	} = $props();

	const fieldId = $props.id();

	let rawValue = $derived(value ?? '');
	let numeric = $derived(isNumericParameter(parameter));
	let allowsExpressions = $derived(parameter.expression !== 'literal');

	// Values that are not plain literals are always shown as text, so that they can be edited.
	let valueNeedsText = $derived(
		rawValue !== '' &&
			(isReferenceValue(rawValue) ||
				(numeric && !Number.isFinite(Number(rawValue))) ||
				(parameter.type === 'enum' && !(parameter.options ?? []).some((o) => o.value === rawValue)) ||
				(parameter.type === 'bool' && !['0', '1', 'true', 'false'].includes(rawValue.toLowerCase()))),
	);
	let expressionToggle = $state(false);
	let expressionMode = $derived(allowsExpressions && (expressionToggle || valueNeedsText));

	let rangeHint = $derived.by(() => {
		if (!numeric) return '';
		const parts: string[] = [];
		if (parameter.min !== undefined && parameter.max !== undefined) parts.push(`${parameter.min} … ${parameter.max}`);
		if (parameter.unit) parts.push(parameter.unit);
		return parts.join(' ');
	});

	let placeholder = $derived.by(() => {
		if (parameter.type === 'enum') {
			const option = parameter.options?.find((o) => o.value === parameter.default);
			return `default: ${option?.displayname || parameter.default}`;
		}
		if (parameter.type === 'bool') return `default: ${parameter.default === '1' || parameter.default === 'true' ? 'on' : 'off'}`;
		return parameter.default !== '' ? `default: ${parameter.default}` : '';
	});

	let boolChecked = $derived.by(() => {
		const effective = (rawValue !== '' ? rawValue : parameter.default).toLowerCase();
		return effective === '1' || effective === 'true';
	});

	const inputClass = $derived([
		'w-full rounded border bg-background/50 px-2 text-sm outline-none focus:ring-1 focus:ring-primary',
		compact ? 'h-7' : 'h-8',
		invalid ? 'border-red-500' : 'border-input',
		readonly && 'opacity-70',
	]);

	function emit(next: string) {
		onchange?.(next.trim() === '' ? undefined : next);
	}

	function toggleExpression() {
		if (expressionMode && valueNeedsText) {
			// Leaving expression mode drops the expression, the default applies again.
			onchange?.(undefined);
		}
		expressionToggle = !expressionMode;
	}
</script>

<div class="flex items-center gap-1">
	{#if expressionMode}
		<input
			id={fieldId}
			type="text"
			class={[inputClass, 'font-mono']}
			value={rawValue}
			{placeholder}
			list={references.length > 0 ? `${fieldId}-references` : undefined}
			disabled={readonly}
			oninput={(e) => emit(e.currentTarget.value)}
		/>
		{#if references.length > 0}
			<datalist id="{fieldId}-references">
				{#each references as reference (reference.name)}
					<option value="${reference.name}">{reference.description}</option>
				{/each}
			</datalist>
		{/if}
	{:else if parameter.type === 'enum'}
		<select
			id={fieldId}
			class={inputClass}
			value={rawValue}
			disabled={readonly}
			onchange={(e) => emit(e.currentTarget.value)}
		>
			<option value="">{placeholder}</option>
			{#each parameter.options ?? [] as option (option.value)}
				<option value={option.value}>{option.displayname || option.value}</option>
			{/each}
		</select>
	{:else if parameter.type === 'bool'}
		<label class={['flex items-center gap-2 text-sm', compact ? 'h-7' : 'h-8']}>
			<input
				id={fieldId}
				type="checkbox"
				checked={boolChecked}
				disabled={readonly}
				onchange={(e) => emit(e.currentTarget.checked ? '1' : '0')}
			/>
			{#if !compact}
				<span class="text-xs text-muted-foreground/60">{rawValue === '' ? placeholder : ''}</span>
			{/if}
		</label>
	{:else}
		<input
			id={fieldId}
			type={numeric ? 'number' : 'text'}
			class={inputClass}
			value={rawValue}
			{placeholder}
			min={numeric ? parameter.min : undefined}
			max={numeric ? parameter.max : undefined}
			step={parameter.type === 'integer' ? 1 : 'any'}
			disabled={readonly}
			oninput={(e) => emit(e.currentTarget.value)}
		/>
	{/if}

	{#if allowsExpressions && !readonly}
		<button
			type="button"
			class={[
				'shrink-0 rounded border px-1 text-[10px] font-mono',
				compact ? 'h-7' : 'h-8',
				expressionMode ? 'border-primary text-primary' : 'border-input text-muted-foreground/60 hover:text-foreground',
			]}
			title={parameter.expression === 'formula' ? 'Enter a formula or a $reference' : 'Enter a $reference'}
			onclick={toggleExpression}
		>
			{parameter.expression === 'formula' ? 'fx' : '$'}
		</button>
	{/if}
</div>
{#if rangeHint && !compact}
	<span class="text-[10px] text-muted-foreground/40">{rangeHint}</span>
{/if}
