<script lang="ts">
	import { onMount } from 'svelte';
	import ShieldCheck from '@lucide/svelte/icons/shield-check';
	import UserPlus from '@lucide/svelte/icons/user-plus';
	import KeyRound from '@lucide/svelte/icons/key-round';
	import CircleCheck from '@lucide/svelte/icons/circle-check';
	import TriangleAlert from '@lucide/svelte/icons/triangle-alert';
	import ArrowLeft from '@lucide/svelte/icons/arrow-left';
	import ArrowRight from '@lucide/svelte/icons/arrow-right';
	import LoaderCircle from '@lucide/svelte/icons/loader-circle';
	import Check from '@lucide/svelte/icons/check';
	import X from '@lucide/svelte/icons/x';
	import LogIn from '@lucide/svelte/icons/log-in';
	import Sun from '@lucide/svelte/icons/sun';
	import Moon from '@lucide/svelte/icons/moon';

	import { usePollTick } from '$lib/amcf/poll.svelte';
	import { theme, themedImageUUID } from '$lib/amcf/theme.svelte';
	import { toggleDarkMode, isDarkMode } from '@core/theme/themeLoader.js';
	import {
		getFirstRunInfo,
		createFirstAdministrator,
		checkPasswordPolicy,
		describeUserError,
		errorCodeOf,
		ERROR_FIRSTRUN_ALREADY_COMPLETED,
		ERROR_INVALID_SETUP_CODE,
		type FirstRunInfo
	} from '$lib/amcf/usersApi';

	let { app, onDone }: { app: any; onDone: () => void } = $props();
	const poll = usePollTick();

	type StepId = 'welcome' | 'account' | 'setupcode' | 'done';

	// Same rule as the server: 1 to 64 characters, starting with a letter or digit.
	const LOGIN_PATTERN = /^[A-Za-z0-9][A-Za-z0-9_.@-]{0,63}$/;

	let info = $state.raw<FirstRunInfo | null>(null);
	let loadError = $state('');
	let step = $state<StepId>('welcome');
	let busy = $state(false);
	let errorMessage = $state('');

	let login = $state('admin');
	let description = $state('');
	let language = $state('');
	let password = $state('');
	let passwordRepeat = $state('');
	let setupCode = $state('');

	let appName = $derived.by(() => { poll.v; return app?.AppDefinition?.TextApplicationName || 'AMCF'; });
	let copyright = $derived.by(() => { poll.v; return app?.AppDefinition?.TextCopyRight || ''; });
	let logoUUID = $derived.by(() => { poll.v; return themedImageUUID(app?.AppDefinition?.LogoUUID, app?.AppDefinition?.DarkLogoUUID); });
	let bgUUID = $derived.by(() => { poll.v; return themedImageUUID(app?.AppDefinition?.LoginBackgroundImageUUID, app?.AppDefinition?.DarkLoginBackgroundImageUUID); });
	let primaryColor = $derived.by(() => { poll.v; return app?.AppDefinition?.Colors?.primary || '#1a1a1a'; });
	let accentColor = $derived.by(() => { poll.v; return app?.AppDefinition?.Colors?.accent || ''; });

	let logoURL = $derived(logoUUID && app ? app.getImageURL(logoUUID) : '');
	let bgStyle = $derived(
		bgUUID && app
			? `background: url("${app.getImageURL(bgUUID)}") center/cover no-repeat;`
			: 'background: linear-gradient(160deg, #1e1e24 0%, #141418 50%, #0c0c0f 100%);'
	);

	function hexLightness(hex: string): number {
		let value = hex.replace('#', '');
		if (value.length === 3) value = value.split('').map((c) => c + c).join('');
		const r = parseInt(value.substring(0, 2), 16) / 255;
		const g = parseInt(value.substring(2, 4), 16) / 255;
		const b = parseInt(value.substring(4, 6), 16) / 255;
		return (Math.max(r, g, b) + Math.min(r, g, b)) / 2;
	}

	let ctaColor = $derived(
		hexLightness(primaryColor) < 0.25 && accentColor && accentColor !== primaryColor ? accentColor : primaryColor
	);
	let ctaTextColor = $derived(hexLightness(ctaColor) > 0.55 ? '#000' : '#fff');

	let steps = $derived.by(() => {
		const list: { id: StepId; label: string }[] = [
			{ id: 'welcome', label: 'Welcome' },
			{ id: 'account', label: 'Administrator' }
		];
		if (info?.setupCodeRequired) list.push({ id: 'setupcode', label: 'Setup code' });
		list.push({ id: 'done', label: 'Done' });
		return list;
	});
	let stepIndex = $derived(steps.findIndex((entry) => entry.id === step));

	let passwordProblems = $derived(info ? checkPasswordPolicy(password, info.passwordPolicy) : []);
	let passwordsMatch = $derived(password.length > 0 && password === passwordRepeat);
	let loginValid = $derived(LOGIN_PATTERN.test(login));
	let accountValid = $derived(loginValid && passwordProblems.length === 0 && passwordsMatch && description.length <= 256);

	// The setup code and the password hash travel unencrypted when another computer uses plain HTTP.
	let insecureTransport = $derived.by(() => {
		if (typeof window === 'undefined') return false;
		const host = window.location.hostname;
		const isLocal = host === 'localhost' || host === '127.0.0.1' || host === '[::1]';
		return window.location.protocol === 'http:' && !isLocal;
	});

	onMount(() => {
		theme.dark = isDarkMode();
		loadInfo();
	});

	async function loadInfo() {
		loadError = '';
		try {
			const result = await getFirstRunInfo(app);
			if (!result.firstRun) {
				app.leaveFirstRun();
				onDone();
				return;
			}
			info = result;
			if (result.languages.length > 0) language = result.languages[0].identifier;
		} catch (err) {
			loadError = describeUserError(app, err);
		}
	}

	function goTo(next: StepId) {
		errorMessage = '';
		step = next;
	}

	function continueFromAccount() {
		if (!accountValid) return;
		if (info?.setupCodeRequired) goTo('setupcode');
		else createAccount();
	}

	async function createAccount() {
		if (!info || busy) return;
		busy = true;
		errorMessage = '';
		try {
			await createFirstAdministrator(app, {
				login,
				description: description.trim(),
				language,
				password,
				setupCode: setupCode.trim()
			});
			passwordRepeat = '';
			setupCode = '';
			step = 'done';
		} catch (err) {
			const code = errorCodeOf(err);
			if (code === ERROR_FIRSTRUN_ALREADY_COMPLETED) {
				app.leaveFirstRun();
				onDone();
				return;
			}
			errorMessage = describeUserError(app, err);
			step = code === ERROR_INVALID_SETUP_CODE ? 'setupcode' : 'account';
		} finally {
			busy = false;
		}
	}

	async function signIn(page: string) {
		if (busy) return;
		busy = true;
		const credentials = { login, password };
		password = '';
		await app.requestLogin(credentials.login, credentials.password);
		if (page && app.AppState.currentStatus === 'ready') app.changePage(page);
		busy = false;
		onDone();
	}

	function handleToggleTheme() {
		theme.dark = toggleDarkMode();
	}
</script>

{#snippet requirement(met: boolean, text: string)}
	<li class="flex items-center gap-1.5 {met ? 'text-emerald-600 dark:text-emerald-400' : 'text-gray-500 dark:text-gray-400'}">
		{#if met}<Check class="h-3.5 w-3.5" />{:else}<X class="h-3.5 w-3.5" />{/if}
		{text}
	</li>
{/snippet}

{#snippet errorBox()}
	{#if errorMessage}
		<div class="flex items-start gap-2 rounded-md border border-red-200 dark:border-red-900/50 bg-red-50 dark:bg-red-950/40 px-3 py-2 text-sm text-red-700 dark:text-red-300">
			<TriangleAlert class="h-4 w-4 mt-0.5 shrink-0" />
			<span>{errorMessage}</span>
		</div>
	{/if}
{/snippet}

<button
	class="fixed top-4 right-4 z-50 p-2.5 rounded-full bg-black/5 dark:bg-white/10 backdrop-blur-sm border border-black/10 dark:border-white/20 text-gray-600 dark:text-white/70 hover:text-gray-900 dark:hover:text-white hover:bg-black/10 dark:hover:bg-white/20 transition shadow-sm"
	onclick={handleToggleTheme}
	title={theme.dark ? 'Switch to light mode' : 'Switch to dark mode'}
>
	{#if theme.dark}<Sun class="h-4 w-4" />{:else}<Moon class="h-4 w-4" />{/if}
</button>

<div class="flex-1 flex items-center justify-center overflow-auto py-8" style={bgStyle}>
	<div class="w-full max-w-[560px] mx-4">
		<div class="rounded-xl bg-white dark:bg-zinc-900 shadow-2xl shadow-black/40 overflow-hidden">
			<div class="h-[3px]" style="background: {ctaColor}"></div>

			<div class="px-8 pt-7 pb-8">
				<div class="flex items-center justify-between gap-4 mb-6">
					{#if logoURL}
						<img src={logoURL} alt={appName} class="max-h-10 max-w-[180px] object-contain" />
					{:else}
						<span class="text-lg font-semibold text-gray-900 dark:text-gray-100">{appName}</span>
					{/if}
					{#if info && stepIndex >= 0}
						<span class="text-xs text-gray-500 dark:text-gray-400">Step {stepIndex + 1} of {steps.length}</span>
					{/if}
				</div>

				{#if info}
					<ol class="flex gap-1 mb-6">
						{#each steps as entry, index (entry.id)}
							<li class="flex-1">
								<div class="h-1 rounded-full {index <= stepIndex ? '' : 'bg-gray-200 dark:bg-zinc-700'}" style={index <= stepIndex ? `background: ${ctaColor}` : ''}></div>
								<div class="mt-1.5 text-[11px] {index === stepIndex ? 'text-gray-900 dark:text-gray-100 font-medium' : 'text-gray-400 dark:text-gray-500'}">{entry.label}</div>
							</li>
						{/each}
					</ol>
				{/if}

				{#if loadError}
					<div class="flex flex-col gap-4">
						<div class="flex items-start gap-2 text-sm text-red-700 dark:text-red-300">
							<TriangleAlert class="h-4 w-4 mt-0.5 shrink-0" />
							<span>{loadError}</span>
						</div>
						<button class="self-start h-9 px-4 rounded-md text-sm font-medium" style="background:{ctaColor}; color:{ctaTextColor}" onclick={loadInfo}>Try again</button>
					</div>
				{:else if !info}
					<div class="flex justify-center py-10">
						<LoaderCircle class="h-6 w-6 animate-spin text-gray-400" />
					</div>
				{:else if !info.available}
					<div class="flex flex-col gap-3 text-sm text-gray-700 dark:text-gray-300">
						<div class="flex items-center gap-2 font-semibold text-gray-900 dark:text-gray-100">
							<TriangleAlert class="h-5 w-5 text-amber-500" /> This machine can not be set up yet
						</div>
						<p>No user account exists, and the configuration does not name a role for the first administrator.</p>
						<p class="text-gray-500 dark:text-gray-400">Ask the machine builder to set <code class="px-1 rounded bg-gray-100 dark:bg-zinc-800">&lt;firstrun adminrole="..."/&gt;</code> in the access control of the machine configuration.</p>
					</div>

				<!-- Step: welcome -->
				{:else if step === 'welcome'}
					<div class="flex flex-col gap-4">
						<div class="flex items-center gap-3">
							<ShieldCheck class="h-7 w-7 shrink-0" style="color:{ctaColor}" />
							<h1 class="text-xl font-semibold text-gray-900 dark:text-gray-100">{info.welcomeTitle || `Welcome to ${appName}`}</h1>
						</div>
						<p class="text-sm leading-relaxed text-gray-600 dark:text-gray-300">
							{info.welcomeText || 'This machine has not been set up yet. You will create the administrator account, which can then add the accounts for everyone else.'}
						</p>
						{#if insecureTransport}
							<div class="flex items-start gap-2 rounded-md border border-amber-200 dark:border-amber-900/50 bg-amber-50 dark:bg-amber-950/40 px-3 py-2 text-sm text-amber-800 dark:text-amber-300">
								<TriangleAlert class="h-4 w-4 mt-0.5 shrink-0" />
								<span>This page was loaded over an unencrypted connection. The setup code and the account data can be read on the network. Prefer setting up the machine on its own screen or over HTTPS.</span>
							</div>
						{/if}
						{#if info.gitHash}
							<p class="text-[11px] text-gray-400 dark:text-gray-500">Server build {info.gitHash}</p>
						{/if}
						<div class="flex justify-end">
							<button class="h-10 px-5 rounded-md text-sm font-medium flex items-center gap-2 hover:brightness-110" style="background:{ctaColor}; color:{ctaTextColor}" onclick={() => goTo('account')}>
								Get started <ArrowRight class="h-4 w-4" />
							</button>
						</div>
					</div>

				<!-- Step: administrator account -->
				{:else if step === 'account'}
					<form class="flex flex-col gap-4" onsubmit={(event) => { event.preventDefault(); continueFromAccount(); }}>
						<div class="flex items-center gap-3">
							<UserPlus class="h-6 w-6 shrink-0" style="color:{ctaColor}" />
							<h1 class="text-lg font-semibold text-gray-900 dark:text-gray-100">Create the administrator account</h1>
						</div>

						{@render errorBox()}

						<label class="flex flex-col gap-1 text-sm">
							<span class="font-medium text-gray-700 dark:text-gray-300">Login name</span>
							<input type="text" bind:value={login} autocomplete="username" maxlength="64"
								class="h-10 rounded-md border border-gray-300 dark:border-zinc-700 bg-white dark:bg-zinc-800 px-3 text-gray-900 dark:text-gray-100 outline-none focus:border-gray-900 dark:focus:border-gray-400" />
							{#if login && !loginValid}
								<span class="text-xs text-red-600 dark:text-red-400">Letters, digits and _ - . @ only, starting with a letter or digit.</span>
							{/if}
						</label>

						<label class="flex flex-col gap-1 text-sm">
							<span class="font-medium text-gray-700 dark:text-gray-300">Full name or description <span class="font-normal text-gray-400">(optional)</span></span>
							<input type="text" bind:value={description} maxlength="256" placeholder="Machine administrator"
								class="h-10 rounded-md border border-gray-300 dark:border-zinc-700 bg-white dark:bg-zinc-800 px-3 text-gray-900 dark:text-gray-100 placeholder:text-gray-400 outline-none focus:border-gray-900 dark:focus:border-gray-400" />
						</label>

						{#if info.languages.length > 0}
							<label class="flex flex-col gap-1 text-sm">
								<span class="font-medium text-gray-700 dark:text-gray-300">Language</span>
								<select bind:value={language}
									class="h-10 rounded-md border border-gray-300 dark:border-zinc-700 bg-white dark:bg-zinc-800 px-3 text-gray-900 dark:text-gray-100 outline-none focus:border-gray-900 dark:focus:border-gray-400">
									{#each info.languages as entry (entry.identifier)}
										<option value={entry.identifier}>{entry.displayName}</option>
									{/each}
								</select>
							</label>
						{/if}

						<label class="flex flex-col gap-1 text-sm">
							<span class="font-medium text-gray-700 dark:text-gray-300">Password</span>
							<input type="password" bind:value={password} autocomplete="new-password"
								class="h-10 rounded-md border border-gray-300 dark:border-zinc-700 bg-white dark:bg-zinc-800 px-3 text-gray-900 dark:text-gray-100 outline-none focus:border-gray-900 dark:focus:border-gray-400" />
						</label>
						<ul class="flex flex-wrap gap-x-4 gap-y-1 text-xs -mt-2">
							{@render requirement(password.length >= info.passwordPolicy.minimumLength, `${info.passwordPolicy.minimumLength}+ characters`)}
							{#if info.passwordPolicy.requireDigit}{@render requirement(/[0-9]/.test(password), 'a digit')}{/if}
							{#if info.passwordPolicy.requireLetter}{@render requirement(/[A-Za-z]/.test(password), 'a letter')}{/if}
							{#if password && password !== password.trim()}{@render requirement(false, 'no spaces at the start or end')}{/if}
						</ul>

						<label class="flex flex-col gap-1 text-sm">
							<span class="font-medium text-gray-700 dark:text-gray-300">Repeat password</span>
							<input type="password" bind:value={passwordRepeat} autocomplete="new-password"
								class="h-10 rounded-md border border-gray-300 dark:border-zinc-700 bg-white dark:bg-zinc-800 px-3 text-gray-900 dark:text-gray-100 outline-none focus:border-gray-900 dark:focus:border-gray-400" />
							{#if passwordRepeat}
								<ul class="text-xs mt-1">{@render requirement(passwordsMatch, passwordsMatch ? 'matches' : 'does not match')}</ul>
							{/if}
						</label>

						{#if info.adminRole}
							<div class="rounded-md bg-gray-50 dark:bg-zinc-800/60 border border-gray-200 dark:border-zinc-700 px-3 py-2.5 text-xs text-gray-600 dark:text-gray-300">
								This account will be <span class="font-semibold text-gray-900 dark:text-gray-100">{info.adminRole.displayName}</span>{#if info.adminRole.permissions.length > 0}, with these rights:{/if}
								{#if info.adminRole.permissions.length > 0}
									<div class="flex flex-wrap gap-1 mt-1.5">
										{#each info.adminRole.permissions as permission (permission.identifier)}
											<span class="rounded-full bg-white dark:bg-zinc-900 border border-gray-200 dark:border-zinc-700 px-2 py-0.5">{permission.displayName}</span>
										{/each}
									</div>
								{/if}
							</div>
						{/if}

						<div class="flex justify-between">
							<button type="button" class="h-10 px-4 rounded-md text-sm text-gray-600 dark:text-gray-300 hover:bg-gray-100 dark:hover:bg-zinc-800 flex items-center gap-2" onclick={() => goTo('welcome')}>
								<ArrowLeft class="h-4 w-4" /> Back
							</button>
							<button type="submit" disabled={!accountValid || busy}
								class="h-10 px-5 rounded-md text-sm font-medium flex items-center gap-2 hover:brightness-110 disabled:opacity-30 disabled:cursor-not-allowed"
								style="background:{ctaColor}; color:{ctaTextColor}">
								{#if busy}<LoaderCircle class="h-4 w-4 animate-spin" />{/if}
								{info.setupCodeRequired ? 'Next' : 'Create account'} <ArrowRight class="h-4 w-4" />
							</button>
						</div>
					</form>

				<!-- Step: setup code -->
				{:else if step === 'setupcode'}
					<form class="flex flex-col gap-4" onsubmit={(event) => { event.preventDefault(); createAccount(); }}>
						<div class="flex items-center gap-3">
							<KeyRound class="h-6 w-6 shrink-0" style="color:{ctaColor}" />
							<h1 class="text-lg font-semibold text-gray-900 dark:text-gray-100">Enter the setup code</h1>
						</div>
						<p class="text-sm leading-relaxed text-gray-600 dark:text-gray-300">
							The server printed a one-time setup code to its console and log when it started, for example <span class="font-mono">7KQ4-M2XD</span>. It proves that you have access to the machine itself.
						</p>

						{@render errorBox()}

						<input type="text" bind:value={setupCode} autocomplete="off" spellcheck="false" maxlength="16" placeholder="XXXX-XXXX"
							class="h-12 rounded-md border border-gray-300 dark:border-zinc-700 bg-white dark:bg-zinc-800 px-3 text-center font-mono text-lg tracking-[0.3em] uppercase text-gray-900 dark:text-gray-100 placeholder:text-gray-300 dark:placeholder:text-zinc-600 outline-none focus:border-gray-900 dark:focus:border-gray-400" />

						<div class="flex justify-between">
							<button type="button" class="h-10 px-4 rounded-md text-sm text-gray-600 dark:text-gray-300 hover:bg-gray-100 dark:hover:bg-zinc-800 flex items-center gap-2" onclick={() => goTo('account')}>
								<ArrowLeft class="h-4 w-4" /> Back
							</button>
							<button type="submit" disabled={setupCode.trim().length < 8 || busy}
								class="h-10 px-5 rounded-md text-sm font-medium flex items-center gap-2 hover:brightness-110 disabled:opacity-30 disabled:cursor-not-allowed"
								style="background:{ctaColor}; color:{ctaTextColor}">
								{#if busy}<LoaderCircle class="h-4 w-4 animate-spin" />{/if}
								Create account
							</button>
						</div>
					</form>

				<!-- Step: done -->
				{:else if step === 'done'}
					<div class="flex flex-col gap-4">
						<div class="flex items-center gap-3">
							<CircleCheck class="h-7 w-7 shrink-0 text-emerald-500" />
							<h1 class="text-lg font-semibold text-gray-900 dark:text-gray-100">The administrator account is ready</h1>
						</div>
						<p class="text-sm text-gray-600 dark:text-gray-300">
							Sign in as <span class="font-semibold text-gray-900 dark:text-gray-100">{login}</span> from now on. The setup page will not be shown again.
						</p>

						{#if info.nextSteps.length > 0}
							<div class="flex flex-col gap-1.5">
								<span class="text-xs font-medium uppercase tracking-wide text-gray-500 dark:text-gray-400">Next steps</span>
								{#each info.nextSteps as nextStep, index (index)}
									<button class="flex items-center justify-between gap-3 rounded-md border border-gray-200 dark:border-zinc-700 px-3 py-2.5 text-left text-sm text-gray-800 dark:text-gray-200 hover:bg-gray-50 dark:hover:bg-zinc-800 disabled:opacity-50"
										disabled={busy || !nextStep.page} onclick={() => signIn(nextStep.page)}>
										<span>{nextStep.text}</span>
										{#if nextStep.page}<ArrowRight class="h-4 w-4 shrink-0 text-gray-400" />{/if}
									</button>
								{/each}
							</div>
						{/if}

						<div class="flex justify-end">
							<button class="h-10 px-5 rounded-md text-sm font-medium flex items-center gap-2 hover:brightness-110 disabled:opacity-30" disabled={busy}
								style="background:{ctaColor}; color:{ctaTextColor}" onclick={() => signIn('')}>
								{#if busy}<LoaderCircle class="h-4 w-4 animate-spin" />{:else}<LogIn class="h-4 w-4" />{/if}
								Continue to {appName}
							</button>
						</div>
					</div>
				{/if}
			</div>
		</div>
		{#if copyright}
			<p class="text-center text-[11px] text-white/35 mt-4">{copyright}</p>
		{/if}
	</div>
</div>
