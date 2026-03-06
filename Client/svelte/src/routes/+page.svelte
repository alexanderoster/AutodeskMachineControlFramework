<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import { Button } from '$lib/components/ui/button/index.js';
	import { Separator } from '$lib/components/ui/separator/index.js';
	import { ScrollArea } from '$lib/components/ui/scroll-area/index.js';
	import * as Sheet from '$lib/components/ui/sheet/index.js';
	import * as Tooltip from '$lib/components/ui/tooltip/index.js';
	import ModuleFactory from '$lib/amcf/ModuleFactory.svelte';
	import LoginPage from '$lib/amcf/LoginPage.svelte';

	import Menu from '@lucide/svelte/icons/menu';
	import PanelLeftClose from '@lucide/svelte/icons/panel-left-close';
	import PanelLeftOpen from '@lucide/svelte/icons/panel-left-open';
	import LogOut from '@lucide/svelte/icons/log-out';

	// @ts-ignore — core JS has no type declarations yet
	import AMCApplication from '@core/common/AMCApplication.js';
	import { initPollTick, type PollTick } from '$lib/amcf/poll.svelte';

	let app: any = $state(null);
	let timer: ReturnType<typeof setInterval> | null = null;
	let drawerOpen = $state(false);
	let sidebarVisible = $state(true);
	let isLargeScreen = $state(false);

	const poll: PollTick = initPollTick();

	/*
	 * AMCApplication is a plain JS class — Svelte 5 cannot track deep
	 * property mutations on it.  Every derived value must reference poll.v
	 * so it re-evaluates whenever the polling cycle bumps the counter.
	 * Child components access this via usePollTick() context instead of props.
	 */
	let status      = $derived.by(() => { poll.v; return app?.AppState?.currentStatus || 'initial'; });
	let menuItems   = $derived.by(() => { poll.v; return [...(app?.AppContent?.MenuItems || [])]; });
	let toolbarItems= $derived.by(() => { poll.v; return [...(app?.AppContent?.ToolbarItems || [])]; });
	let pages       = $derived.by(() => { poll.v; return [...(app?.AppContent?.Pages || [])]; });
	let dialogs     = $derived.by(() => { poll.v; return [...(app?.AppContent?.Dialogs || [])]; });
	let appName     = $derived.by(() => { poll.v; return app?.AppDefinition?.TextApplicationName || 'AMCF'; });
	let copyright   = $derived.by(() => { poll.v; return app?.AppDefinition?.TextCopyRight || ''; });
	let toolbarLogoUUID = $derived.by(() => { poll.v; return app?.AppDefinition?.ToolbarLogoUUID || ''; });

	function bumpTick () { poll.v++; }

	let mql: MediaQueryList | null = null;
	function handleScreenChange (e: MediaQueryListEvent | MediaQueryList) {
		isLargeScreen = e.matches;
	}

	onMount(() => {
		mql = window.matchMedia('(min-width: 1024px)');
		isLargeScreen = mql.matches;
		mql.addEventListener('change', handleScreenChange);

		let baseURL = '/api';
		if (import.meta.env.DEV) {
			baseURL = 'http://localhost:8869/api';
		}

		app = new AMCApplication(baseURL, bumpTick);
		app.retrieveConfiguration(null);

		// Bump tick shortly after config fetch so login screen appears
		setTimeout(bumpTick, 1200);

		timer = setInterval(() => {
			if (app) {
				app.retrieveFrontendState()
					.finally(() => {
						app.updateModules();
						poll.v++;
					});
			}
		}, 600);
	});

	onDestroy(() => {
		if (timer) clearInterval(timer);
		if (mql) mql.removeEventListener('change', handleScreenChange);
	});

	function menuClick (item: any) {
		if (item.targetpage) app.changePage(item.targetpage);
		if (item.eventname) app.triggerUIEvent(item.eventname, item.uuid, {});
		if (!isLargeScreen) drawerOpen = false;
	}

	function toolbarClick (item: any) {
		if (item.targetpage) app.changePage(item.targetpage);
		if (item.eventname) app.triggerUIEvent(item.eventname, item.uuid, {});
	}

	function toggleSidebar () {
		if (isLargeScreen) {
			sidebarVisible = !sidebarVisible;
		} else {
			drawerOpen = !drawerOpen;
		}
	}
</script>

<!-- Loading -->
{#if status === 'initial'}
	<div class="flex-1 flex items-center justify-center">
		<div class="animate-spin h-8 w-8 border-2 border-primary border-t-transparent rounded-full"></div>
	</div>

<!-- Login -->
{:else if status === 'login'}
	<LoginPage {app} onLogin={() => setTimeout(bumpTick, 800)} />

<!-- Main Application -->
{:else if status === 'ready'}
	<!-- Header bar -->
	<header class="h-12 bg-[#3C3C3C] text-white flex items-center px-3 gap-2 shrink-0">
		<!-- Hamburger / sidebar toggle -->
		<button class="p-2 hover:bg-white/10 rounded" onclick={toggleSidebar}>
			{#if isLargeScreen}
				{#if sidebarVisible}
					<PanelLeftClose class="h-5 w-5" />
				{:else}
					<PanelLeftOpen class="h-5 w-5" />
				{/if}
			{:else}
				<Menu class="h-5 w-5" />
			{/if}
		</button>

		<!-- Mobile overlay drawer -->
		{#if !isLargeScreen}
			<Sheet.Root bind:open={drawerOpen}>
				<Sheet.Content side="left" class="w-72 p-0">
					<Sheet.Header class="p-4 border-b">
						<Sheet.Title>{appName}</Sheet.Title>
					</Sheet.Header>
					<ScrollArea class="flex-1">
						<nav class="flex flex-col">
							{#each menuItems as item (item.uuid)}
								<button
									class="flex items-start gap-3 px-4 py-3 text-left hover:bg-accent border-b border-border/50 transition-colors"
									onclick={() => menuClick(item)}
								>
									<div class="mt-0.5 text-muted-foreground text-sm">●</div>
									<div>
										<div class="text-sm font-medium">{item.caption}</div>
										<div class="text-xs text-muted-foreground">{item.description || ''}</div>
									</div>
								</button>
							{/each}
						</nav>
					</ScrollArea>
				</Sheet.Content>
			</Sheet.Root>
		{/if}

		<!-- Toolbar logo / app name -->
		<button
			class="text-sm font-medium px-3 py-1.5 hover:bg-white/10 rounded"
			onclick={() => app.changePage(app.AppDefinition.MainPage)}
		>
			{#if toolbarLogoUUID}
				<img src={app.getImageURL(toolbarLogoUUID)} alt={appName} class="h-6 max-w-[150px] object-contain" />
			{:else}
				<span>{appName}</span>
			{/if}
		</button>

		<div class="flex-1"></div>

		{#each toolbarItems as item (item.uuid)}
			<Tooltip.Root>
				<Tooltip.Trigger>
					{#snippet child({ props })}
						<button
							{...props}
							class="text-sm px-3 py-1.5 hover:bg-white/10 rounded flex items-center gap-1.5"
							onclick={() => toolbarClick(item)}
						>
							{#if item.icon === 'mdi-logout'}
								<LogOut class="h-4 w-4" />
							{/if}
							{item.caption}
						</button>
					{/snippet}
				</Tooltip.Trigger>
				<Tooltip.Content>{item.caption}</Tooltip.Content>
			</Tooltip.Root>
		{/each}
	</header>

	<!-- Body: permanent sidebar + page content -->
	<div class="flex flex-1 min-h-0">
		<!-- Permanent sidebar (large screens) -->
		{#if isLargeScreen && sidebarVisible}
			<aside class="w-72 border-r border-border bg-card shrink-0 flex flex-col">
				<div class="p-4 border-b font-semibold text-sm">{appName}</div>
				<ScrollArea class="flex-1">
					<nav class="flex flex-col">
						{#each menuItems as item (item.uuid)}
							<button
								class="flex items-start gap-3 px-4 py-3 text-left hover:bg-accent border-b border-border/50 transition-colors"
								onclick={() => menuClick(item)}
							>
								<div class="mt-0.5 text-muted-foreground text-sm">●</div>
								<div>
									<div class="text-sm font-medium">{item.caption}</div>
									<div class="text-xs text-muted-foreground">{item.description || ''}</div>
								</div>
							</button>
						{/each}
					</nav>
				</ScrollArea>
			</aside>
		{/if}

		<!-- Page content -->
		<main class="flex-1 min-w-0 overflow-auto p-2">
			{#each pages as page (page.name)}
				{#if poll.v >= 0 && app.pageIsActive(page)}
					<div class="w-full h-full">
						{#each page.modules || [] as mod (mod.uuid)}
							<ModuleFactory module={mod} {app} />
						{/each}
					</div>
				{/if}
			{/each}
		</main>
	</div>

	<!-- Footer -->
	<footer class="h-8 bg-[#3C3C3C] text-white/60 flex items-center justify-end px-4 text-xs shrink-0">
		{#if copyright}
			&copy; {copyright}
		{/if}
	</footer>

<!-- Error -->
{:else if status === 'error'}
	<div class="flex-1 flex items-center justify-center">
		<div class="bg-card border border-destructive/25 rounded-lg p-8 text-center max-w-md">
			<h2 class="text-lg font-semibold text-destructive mb-2">Connection Error</h2>
			<p class="text-sm text-muted-foreground">Unable to connect to the AMCF server.</p>
		</div>
	</div>
{/if}
