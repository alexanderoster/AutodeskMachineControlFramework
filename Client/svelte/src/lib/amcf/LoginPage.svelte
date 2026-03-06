<script lang="ts">
	import { Button } from '$lib/components/ui/button/index.js';
	import * as Card from '$lib/components/ui/card/index.js';
	import { Input } from '$lib/components/ui/input/index.js';
	import { Label } from '$lib/components/ui/label/index.js';
	import LogIn from '@lucide/svelte/icons/log-in';
	import ArrowRight from '@lucide/svelte/icons/arrow-right';
	import ArrowLeft from '@lucide/svelte/icons/arrow-left';
	import User from '@lucide/svelte/icons/user';
	import Lock from '@lucide/svelte/icons/lock';

	import { usePollTick } from '$lib/amcf/poll.svelte';

	let { app, onLogin }: { app: any; onLogin: () => void } = $props();
	const poll = usePollTick();

	let loginUser = $state('');
	let loginPassword = $state('');
	let userStep = $state(true);

	let loginStyle   = $derived.by(() => { poll.v; return (app?.AppDefinition?.LoginStyle || 'classic').toLowerCase(); });
	let appName      = $derived.by(() => { poll.v; return app?.AppDefinition?.TextApplicationName || 'AMCF'; });
	let welcomeMsg   = $derived.by(() => { poll.v; return app?.AppDefinition?.LoginWelcomeMessage || ''; });
	let subtitle     = $derived.by(() => { poll.v; return app?.AppDefinition?.LoginSubtitle || ''; });
	let logoUUID     = $derived.by(() => { poll.v; return app?.AppDefinition?.LogoUUID || ''; });
	let logoAspect   = $derived.by(() => { poll.v; return app?.AppDefinition?.LogoAspectRatio || 3; });
	let panelUUID    = $derived.by(() => { poll.v; return app?.AppDefinition?.LoginPanelUUID || ''; });
	let bgUUID       = $derived.by(() => { poll.v; return app?.AppDefinition?.LoginBackgroundImageUUID || ''; });
	let primaryColor = $derived.by(() => { poll.v; return app?.AppDefinition?.Colors?.primary || '#C8102E'; });

	let logoURL     = $derived(logoUUID && app ? app.getImageURL(logoUUID) : '');
	let panelURL    = $derived(panelUUID && app ? app.getImageURL(panelUUID) : '');
	let bgURL       = $derived(bgUUID && app ? app.getImageURL(bgUUID) : '');
	let bgStyle     = $derived(bgURL ? `background-image: url("${bgURL}"); background-size: cover; background-position: center;` : '');

	function doLogin () {
		if (loginUser && loginPassword && app) {
			app.requestLogin(loginUser, loginPassword);
			loginPassword = '';
			onLogin();
		}
	}

	function nextStep () {
		if (loginUser) userStep = false;
	}

	function backStep () {
		userStep = true;
		loginPassword = '';
	}

	function onEnter () {
		if (loginUser && loginPassword) doLogin();
	}
</script>

<!-- ========== CLASSIC ========== -->
{#if loginStyle === 'classic'}
	<div class="flex-1 flex items-center justify-center" style={bgStyle}>
		<div class="w-full max-w-sm">
			<Card.Root>
				<Card.Header class="bg-primary text-primary-foreground rounded-t-lg">
					<Card.Title class="text-primary-foreground">{appName}</Card.Title>
				</Card.Header>
				<Card.Content class="pt-6 flex flex-col items-center gap-4">
					{#if logoURL}
						<img src={logoURL} alt="Logo" class="max-h-12 object-contain" style="aspect-ratio: {logoAspect}" />
					{/if}
					{#if welcomeMsg}
						<p class="text-sm text-center text-muted-foreground">{welcomeMsg}</p>
					{/if}

					{#if userStep}
						<div class="w-full flex flex-col gap-3 mt-2">
							<div class="flex items-center gap-2">
								<User class="h-4 w-4 text-muted-foreground shrink-0" />
								<Input placeholder="User name" bind:value={loginUser} onkeydown={(e: KeyboardEvent) => e.key === 'Enter' && nextStep()} />
							</div>
							<Button onclick={nextStep} disabled={!loginUser} variant="outline" class="w-full">
								Next <ArrowRight class="h-4 w-4 ml-1" />
							</Button>
						</div>
					{:else}
						<div class="w-full flex flex-col gap-3 mt-2">
							<div class="flex items-center gap-2">
								<Lock class="h-4 w-4 text-muted-foreground shrink-0" />
								<Input type="password" placeholder="Password" bind:value={loginPassword} onkeydown={(e: KeyboardEvent) => e.key === 'Enter' && doLogin()} />
							</div>
							<div class="flex gap-2">
								<Button onclick={backStep} variant="outline" class="flex-1">
									<ArrowLeft class="h-4 w-4 mr-1" /> Back
								</Button>
								<Button onclick={doLogin} disabled={!loginPassword} class="flex-1">
									<LogIn class="h-4 w-4 mr-1" /> Login
								</Button>
							</div>
						</div>
					{/if}
				</Card.Content>
			</Card.Root>
		</div>
	</div>

<!-- ========== INDUSTRIAL (dark glass) ========== -->
{:else if loginStyle === 'industrial'}
	<div class="flex-1 flex items-center justify-center" style={bgStyle}>
		<div class="w-full max-w-sm mx-4">
			<div class="rounded-2xl p-10 backdrop-blur-xl bg-[rgba(20,20,30,0.72)] border border-white/12 text-white">
				<div class="text-center mb-7">
					{#if logoURL}
						<img src={logoURL} alt="Logo" class="max-h-12 mx-auto mb-4 object-contain" />
					{/if}
					<h1 class="text-2xl font-semibold tracking-wide">{appName}</h1>
					{#if subtitle}<p class="text-sm opacity-75 mt-1">{subtitle}</p>{/if}
					{#if welcomeMsg}<p class="text-xs opacity-60 mt-2 leading-relaxed">{welcomeMsg}</p>{/if}
				</div>
				<div class="flex flex-col gap-4 max-w-[360px] mx-auto">
					<input type="text" placeholder="User name" bind:value={loginUser}
						class="h-10 w-full rounded-md border border-white/25 bg-transparent px-3 text-sm text-white placeholder:text-white/50 focus:outline-none focus:ring-1 focus:ring-white/40"
						onkeydown={(e) => e.key === 'Enter' && onEnter()} />
					<input type="password" placeholder="Password" bind:value={loginPassword}
						class="h-10 w-full rounded-md border border-white/25 bg-transparent px-3 text-sm text-white placeholder:text-white/50 focus:outline-none focus:ring-1 focus:ring-white/40"
						onkeydown={(e) => e.key === 'Enter' && onEnter()} />
					<Button onclick={doLogin} disabled={!loginUser || !loginPassword} class="w-full mt-2">
						<LogIn class="h-4 w-4 mr-2" /> Sign In
					</Button>
				</div>
			</div>
		</div>
	</div>

<!-- ========== LUMINOUS (light glass) ========== -->
{:else if loginStyle === 'luminous'}
	<div class="flex-1 flex items-center justify-center" style={bgStyle}>
		<div class="w-full max-w-sm mx-4">
			<div class="rounded-2xl p-10 backdrop-blur-xl bg-white/78 border border-black/8 shadow-lg text-foreground">
				<div class="text-center mb-7">
					{#if logoURL}
						<img src={logoURL} alt="Logo" class="max-h-12 mx-auto mb-4 object-contain" />
					{/if}
					<h1 class="text-2xl font-semibold tracking-wide">{appName}</h1>
					{#if subtitle}<p class="text-sm text-muted-foreground mt-1">{subtitle}</p>{/if}
					{#if welcomeMsg}<p class="text-xs text-muted-foreground/70 mt-2 leading-relaxed">{welcomeMsg}</p>{/if}
				</div>
				<div class="flex flex-col gap-4 max-w-[360px] mx-auto">
					<Input placeholder="User name" bind:value={loginUser} onkeydown={(e: KeyboardEvent) => e.key === 'Enter' && onEnter()} />
					<Input type="password" placeholder="Password" bind:value={loginPassword} onkeydown={(e: KeyboardEvent) => e.key === 'Enter' && onEnter()} />
					<Button onclick={doLogin} disabled={!loginUser || !loginPassword} class="w-full mt-2">
						<LogIn class="h-4 w-4 mr-2" /> Sign In
					</Button>
				</div>
			</div>
		</div>
	</div>

<!-- ========== SPLIT ========== -->
{:else if loginStyle === 'split'}
	<div class="flex-1 flex flex-row h-full" style={bgStyle}>
		<!-- Brand panel (hidden on mobile) -->
		<div class="hidden md:flex w-5/12 items-center justify-center text-white relative overflow-hidden"
			style="background-color: {primaryColor}">
			<div class="relative z-10 text-center px-10 max-w-[420px]">
				{#if logoURL}
					<img src={logoURL} alt="Logo" class="max-h-14 mx-auto mb-6 object-contain" />
				{/if}
				<h1 class="text-3xl font-bold tracking-wide mb-2">{appName}</h1>
				{#if subtitle}<p class="text-lg opacity-85 mb-1">{subtitle}</p>{/if}
				{#if welcomeMsg}<p class="text-sm opacity-70 leading-relaxed">{welcomeMsg}</p>{/if}
				{#if panelURL}
					<img src={panelURL} alt="" class="mt-8 max-h-48 mx-auto object-contain opacity-90" />
				{/if}
			</div>
		</div>
		<!-- Form panel -->
		<div class="flex-1 flex items-center justify-center bg-white/92 backdrop-blur-xl border-l border-black/8">
			<div class="w-full max-w-[400px] px-8 py-12 text-center">
				{#if logoURL}
					<img src={logoURL} alt="Logo" class="max-h-12 mx-auto mb-4 object-contain md:hidden" />
				{/if}
				<h1 class="text-2xl font-semibold text-foreground">Sign In</h1>
				<p class="text-sm text-muted-foreground mt-1 mb-6">Enter your credentials to continue</p>
				<div class="flex flex-col gap-4">
					<Input placeholder="User name" bind:value={loginUser} onkeydown={(e: KeyboardEvent) => e.key === 'Enter' && onEnter()} />
					<Input type="password" placeholder="Password" bind:value={loginPassword} onkeydown={(e: KeyboardEvent) => e.key === 'Enter' && onEnter()} />
					<Button onclick={doLogin} disabled={!loginUser || !loginPassword} class="w-full mt-2">
						<LogIn class="h-4 w-4 mr-2" /> Sign In
					</Button>
				</div>
			</div>
		</div>
	</div>

<!-- ========== FALLBACK (same as classic) ========== -->
{:else}
	<div class="flex-1 flex items-center justify-center" style={bgStyle}>
		<div class="w-full max-w-sm">
			<Card.Root>
				<Card.Header class="bg-primary text-primary-foreground rounded-t-lg">
					<Card.Title class="text-primary-foreground">{appName}</Card.Title>
				</Card.Header>
				<Card.Content class="pt-6 flex flex-col items-center gap-4">
					{#if logoURL}
						<img src={logoURL} alt="Logo" class="max-h-12 object-contain" />
					{/if}
					{#if welcomeMsg}
						<p class="text-sm text-center text-muted-foreground">{welcomeMsg}</p>
					{/if}
					<div class="w-full flex flex-col gap-3 mt-2">
						<Input placeholder="User name" bind:value={loginUser} onkeydown={(e: KeyboardEvent) => e.key === 'Enter' && onEnter()} />
						<Input type="password" placeholder="Password" bind:value={loginPassword} onkeydown={(e: KeyboardEvent) => e.key === 'Enter' && onEnter()} />
						<Button onclick={doLogin} disabled={!loginUser || !loginPassword} class="w-full">
							<LogIn class="h-4 w-4 mr-2" /> Sign In
						</Button>
					</div>
				</Card.Content>
			</Card.Root>
		</div>
	</div>
{/if}
