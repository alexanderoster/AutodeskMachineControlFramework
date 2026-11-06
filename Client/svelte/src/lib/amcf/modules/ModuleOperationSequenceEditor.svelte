<script lang="ts">
	import { useModuleTick } from '$lib/amcf/poll.svelte';
	import { Button } from '$lib/components/ui/button';
	import { Separator } from '$lib/components/ui/separator';
	import WorkflowEditor from './workflow/WorkflowEditor.svelte';
	import type {
		OperationSequenceData,
		OperationTypeEntry,
		OperationSequenceSummary,
		BindingReference,
		ValidationIssue,
		ValidationResult,
	} from './workflow/workflowTypes';
	import { createWorkflowState, resetWorkflowState } from './workflow/workflowStore';

	let { module, app }: { module: any; app: any } = $props();
	const poll = useModuleTick(() => module);

	const VALIDATION_DEBOUNCE_MS = 600;

	const emptySequence: OperationSequenceData = {
		name: '',
		displayname: '',
		description: '',
		context: '',
		bindinggroup: '',
		revision: 0,
		operations: [],
	};

	let visible = $derived.by(() => { poll.v; return module.visible !== false; });
	let catalog: OperationTypeEntry[] = $derived.by(() => { poll.v; return Array.isArray(module.catalog) ? module.catalog : []; });
	let sequences: OperationSequenceSummary[] = $derived.by(() => { poll.v; return Array.isArray(module.sequences) ? module.sequences : []; });
	let referenceMap: Record<string, BindingReference[]> = $derived.by(() => { poll.v; return module.references ?? {}; });
	let canEdit = $derived.by(() => { poll.v; return module.canedit === true; });
	let activeOperation = $derived.by(() => { poll.v; return String(module.activeoperation ?? ''); });
	let activeSequence = $derived.by(() => { poll.v; return String(module.activesequence ?? ''); });
	let activeRevision = $derived.by(() => { poll.v; return Number(module.activerevision) || 0; });

	let workflowState = $state(createWorkflowState(emptySequence));
	let selectedName = $state('');
	let latestRevision = $state(0);
	let issues: ValidationIssue[] = $state.raw([]);
	let busy = $state(false);
	let message = $state('');
	let messageIsError = $state(false);
	let validationTimer: ReturnType<typeof setTimeout> | undefined;

	let readonly = $derived(!canEdit || busy || selectedName === '');
	let references = $derived(referenceMap[selectedName] ?? []);
	let errorCount = $derived(issues.filter((issue) => issue.severity === 'error').length);
	let isRunning = $derived(activeSequence !== '' && activeSequence === selectedName);
	let selectedSummary = $derived(sequences.find((s) => s.name === selectedName));

	// Opens the first sequence once the list has arrived. Only tried once, so that a failing load does not repeat.
	let initialLoadStarted = false;
	$effect(() => {
		if (!initialLoadStarted && sequences.length > 0) {
			initialLoadStarted = true;
			loadSequence(sequences[0].name);
		}
	});

	$effect(() => () => clearTimeout(validationTimer));

	function iconURL(icon: string): string {
		return app?.getImageURL ? app.getImageURL(icon) : '';
	}

	function showMessage(text: string, isError: boolean = false) {
		message = text;
		messageIsError = isError;
	}

	function errorText(err: any): string {
		const data = err?.response?.data;
		if (data && typeof data.message === 'string') return data.message;
		if (app?.extractErrorMessage) return app.extractErrorMessage(err);
		return String(err?.message ?? err);
	}

	async function request(requestType: string, payload: Record<string, unknown>): Promise<any> {
		const response = await app.axiosPostRequest(`/widget/${module.uuid}/${requestType}`, payload);
		return response.data;
	}

	async function run(action: () => Promise<void>) {
		busy = true;
		try {
			await action();
		} catch (err) {
			showMessage(errorText(err), true);
		} finally {
			busy = false;
		}
	}

	function applyValidation(validation: ValidationResult | undefined) {
		issues = validation?.issues ?? [];
	}

	function loadSequence(name: string) {
		return run(async () => {
			const data = await request('loadsequence', { name });
			selectedName = name;
			latestRevision = data.latestrevision ?? data.sequence.revision;
			resetWorkflowState(workflowState, data.sequence);
			applyValidation(data.validation);
			showMessage('');
		});
	}

	function selectSequence(name: string) {
		if (name === selectedName) return;
		if (workflowState.dirty && !confirm('Discard the unsaved changes?')) return;
		loadSequence(name);
	}

	function scheduleValidation() {
		clearTimeout(validationTimer);
		validationTimer = setTimeout(validateNow, VALIDATION_DEBOUNCE_MS);
	}

	async function validateNow() {
		clearTimeout(validationTimer);
		if (selectedName === '') return;
		try {
			const data = await request('validatesequence', {
				sequence: $state.snapshot(workflowState.sequence),
				baserevision: workflowState.baseRevision,
			});
			applyValidation(data.validation);
		} catch (err) {
			showMessage(errorText(err), true);
		}
	}

	function save() {
		return run(async () => {
			clearTimeout(validationTimer);
			const data = await request('savesequence', {
				sequence: $state.snapshot(workflowState.sequence),
				baserevision: workflowState.baseRevision,
			});
			if (data.status === 'saved') {
				latestRevision = data.sequence.revision;
				resetWorkflowState(workflowState, data.sequence);
				issues = [];
				showMessage(`Saved as revision ${data.sequence.revision}`);
			} else if (data.status === 'invalid') {
				applyValidation(data.validation);
				showMessage('Not saved: the sequence has errors', true);
			} else if (data.status === 'conflict') {
				latestRevision = data.latestrevision;
				showMessage(`Not saved: revision ${data.latestrevision} was published in the meantime. Revert to load it.`, true);
			}
		});
	}

	function revert() {
		if (workflowState.dirty && !confirm('Discard the unsaved changes?')) return;
		loadSequence(selectedName);
	}

	function resetToDefault() {
		if (!confirm('Replace the editor content with the factory default? Nothing is saved until you save.')) return;
		run(async () => {
			const data = await request('resetsequence', { name: selectedName });
			resetWorkflowState(workflowState, { ...data.sequence, revision: latestRevision }, true);
			applyValidation(data.validation);
			showMessage('Factory default loaded. Save to publish it as a new revision.');
		});
	}

	function exportSequence() {
		run(async () => {
			const payload: Record<string, unknown> = { name: selectedName };
			if (latestRevision !== 0) payload.revision = latestRevision;
			const data = await request('exportsequence', payload);
			const blob = new Blob([data.xml], { type: 'application/xml' });
			const url = URL.createObjectURL(blob);
			const link = document.createElement('a');
			link.href = url;
			link.download = `${selectedName}_rev${latestRevision}.xml`;
			link.click();
			URL.revokeObjectURL(url);
		});
	}

	let importInput: HTMLInputElement | undefined = $state();

	function importSequence(file: File | undefined) {
		if (!file) return;
		run(async () => {
			const xml = await file.text();
			const data = await request('importsequence', { xml, name: selectedName });
			resetWorkflowState(workflowState, data.sequence, true);
			applyValidation(data.validation);
			showMessage(`Imported ${file.name}. Save to publish it.`);
		});
		if (importInput) importInput.value = '';
	}
</script>

{#snippet toolbar()}
	<select
		class="h-8 rounded border border-input bg-background/50 px-2 text-sm"
		value={selectedName}
		disabled={busy}
		onchange={(e) => selectSequence(e.currentTarget.value)}
	>
		{#if selectedName === ''}
			<option value="">Select a sequence…</option>
		{/if}
		{#each sequences as sequence (sequence.name)}
			<option value={sequence.name}>{sequence.displayname || sequence.name}</option>
		{/each}
	</select>

	{#if selectedName !== ''}
		<span class="text-xs text-muted-foreground tabular-nums">
			revision {latestRevision}
			{#if isRunning}
				· <span class="text-green-700 dark:text-green-400">running revision {activeRevision}</span>
			{/if}
			{#if workflowState.dirty}
				· <span class="text-amber-700 dark:text-amber-400">unsaved</span>
			{/if}
			{#if !canEdit}
				· read only
			{/if}
		</span>

		<Separator orientation="vertical" class="h-5" />

		<Button variant="outline" size="sm" disabled={busy} onclick={validateNow}>Validate</Button>
		{#if canEdit}
			<Button size="sm" disabled={busy || !workflowState.dirty || errorCount > 0} onclick={save}>Save</Button>
			<Button variant="ghost" size="sm" disabled={busy || !workflowState.dirty} onclick={revert}>Revert</Button>
			{#if selectedSummary?.hasdefault}
				<Button variant="ghost" size="sm" disabled={busy} onclick={resetToDefault}>Reset to default</Button>
			{/if}
		{/if}

		<Button variant="ghost" size="sm" disabled={busy} onclick={exportSequence}>Export</Button>
		{#if canEdit}
			<Button variant="ghost" size="sm" disabled={busy} onclick={() => importInput?.click()}>Import</Button>
			<input
				bind:this={importInput}
				type="file"
				accept=".xml,application/xml,text/xml"
				class="hidden"
				onchange={(e) => importSequence(e.currentTarget.files?.[0])}
			/>
		{/if}

		{#if errorCount > 0}
			<span class="text-xs text-red-700 dark:text-red-400">{errorCount} {errorCount === 1 ? 'error' : 'errors'}</span>
		{/if}
	{/if}
{/snippet}

{#if visible}
	<div class="w-full flex flex-col gap-2" style="min-height: 500px; height: 70vh;">
		{#if message}
			<div
				class={[
					'rounded border px-3 py-1.5 text-xs',
					messageIsError ? 'border-red-500/40 text-red-700 dark:text-red-300' : 'border-input text-muted-foreground',
				]}
			>
				{message}
			</div>
		{/if}

		<div class="flex-1 min-h-0">
			<WorkflowEditor
				bind:state={workflowState}
				{catalog}
				{readonly}
				{issues}
				activeOperation={isRunning && activeRevision === latestRevision ? activeOperation : ''}
				{references}
				{iconURL}
				{toolbar}
				onchange={scheduleValidation}
			/>
		</div>
	</div>
{/if}
