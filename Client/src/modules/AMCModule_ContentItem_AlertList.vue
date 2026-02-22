<!--

Copyright (C) 2020 Autodesk Inc.

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
	* Redistributions of source code must retain the above copyright
	  notice, this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright
	  notice, this list of conditions and the following disclaimer in the
	  documentation and/or other materials provided with the distribution.
	* Neither the name of the Autodesk Inc. nor the
	  names of its contributors may be used to endorse or promote products
	  derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL AUTODESK INC. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

!-->

<template>
<div v-if="moduleitem.type === 'alertlist'" class="al-root">

	<div class="al-area">

		<!-- Sticky header -->
		<div class="al-header">
			<table class="al-table">
				<colgroup>
					<col style="width: 110px" />
					<col />
					<col style="width: 200px" />
					<col style="width: 110px" />
					<col style="width: 70px" />
				</colgroup>
				<thead>
					<tr>
						<th>Time</th>
						<th>Alert</th>
						<th>Context</th>
						<th>Level</th>
						<th>Active</th>
					</tr>
				</thead>
			</table>
		</div>

		<!-- Scrollable body -->
		<div class="al-scroll">
			<table class="al-table">
				<colgroup>
					<col style="width: 110px" />
					<col />
					<col style="width: 200px" />
					<col style="width: 110px" />
					<col style="width: 70px" />
				</colgroup>
				<tbody>

					<!-- Empty state -->
					<tr v-if="!moduleitem.entries || moduleitem.entries.length === 0">
						<td colspan="5" class="al-cell-empty">
							<div class="al-empty-state">
								<v-icon color="grey lighten-1" size="36">mdi-bell-off-outline</v-icon>
								<span class="al-empty-title">No alerts</span>
							</div>
						</td>
					</tr>

					<!-- Data rows -->
					<tr
						v-for="item in moduleitem.entries"
						:key="item.alertuuid"
						class="al-row"
						:class="{ 'al-row--active': item.alertactive }"
						@click="onRowClick(item)"
					>
						<!-- Timestamp -->
						<td class="al-cell al-cell-time">{{ formatTime(item.alerttimestamp) }}</td>

						<!-- Alert caption + identifier subtitle -->
						<td class="al-cell">
							<div class="al-caption-cell">
								<span class="al-caption-text">{{ item.alertcaption }}</span>
								<span v-if="item.alertidentifier" class="al-sub-text">{{ item.alertidentifier }}</span>
							</div>
						</td>

						<!-- Context -->
						<td class="al-cell al-cell-context">{{ item.alertcontext }}</td>

						<!-- Severity badge -->
						<td class="al-cell">
							<span v-if="item.severity || item.alertlevel" class="al-badge" :class="severityClass(item.severity || item.alertlevel)">
								{{ item.severity || item.alertlevel }}
							</span>
						</td>

						<!-- Active indicator -->
						<td class="al-cell al-cell-active">
							<v-icon v-if="item.alertactive" small color="red darken-1">mdi-alert-circle</v-icon>
							<v-icon v-else small color="grey lighten-1">mdi-check-circle-outline</v-icon>
						</td>
					</tr>

				</tbody>
			</table>
		</div>

	</div>

</div>
</template>

<script>
const SEVERITY_CLASSES = {
	error:    'al-badge--red',
	critical: 'al-badge--red',
	warning:  'al-badge--yellow',
	warn:     'al-badge--yellow',
	info:     'al-badge--blue',
	debug:    'al-badge--slate',
	ok:       'al-badge--green',
	fatal:    'al-badge--red',
};

export default {
	props: ['Application', 'moduleitem'],

	methods: {
		severityClass(level) {
			return SEVERITY_CLASSES[(level || '').toLowerCase()] || 'al-badge--slate';
		},

		formatTime(timeString) {
			if (!timeString) return '';
			const date = new Date(timeString);
			if (isNaN(date.getTime())) return timeString;
			return new Intl.DateTimeFormat('en-US', {
				month:  'short',
				day:    'numeric',
				hour:   '2-digit',
				minute: '2-digit',
				second: '2-digit',
				hour12: false,
			}).format(date);
		},

		onRowClick(item) {
			if (item && this.moduleitem.selectevent && this.moduleitem.selectionvalueuuid) {
				const eventValues = {};
				eventValues[this.moduleitem.selectionvalueuuid] = item.alertuuid;
				this.Application.triggerUIEvent(this.moduleitem.selectevent, this.moduleitem.uuid, eventValues);
			}
		},
	},
};
</script>

<style scoped>
/* ── Root ────────────────────────────────────────────────── */
.al-root {
	width: 100%;
	display: flex;
	flex-direction: column;
}

.al-area {
	width: 100%;
	display: flex;
	flex-direction: column;
}

/* ── Header ──────────────────────────────────────────────── */
.al-header {
	border: 1px solid rgba(0, 0, 0, 0.1);
	border-bottom: none;
	border-radius: 6px 6px 0 0;
	overflow: hidden;
}

/* ── Scrollable body ─────────────────────────────────────── */
.al-scroll {
	overflow-x: hidden;
	border: 1px solid rgba(0, 0, 0, 0.1);
	border-top: none;
	border-radius: 0 0 6px 6px;
}

/* ── Shared table ────────────────────────────────────────── */
.al-table {
	width: 100%;
	border-collapse: collapse;
	table-layout: fixed;
}

.al-table thead th {
	background: #fafafa;
	text-align: left;
	padding: 6px 12px;
	font-size: 0.75rem;
	font-weight: 600;
	color: rgba(0, 0, 0, 0.55);
	text-transform: none;
	letter-spacing: 0;
	border-bottom: 1px solid rgba(0, 0, 0, 0.1);
	white-space: nowrap;
}

/* ── Body rows ───────────────────────────────────────────── */
.al-row {
	cursor: pointer;
}

.al-row:hover {
	background: rgba(0, 0, 0, 0.03);
}

.al-row--active {
	background: rgba(220, 38, 38, 0.03);
}

.al-row--active:hover {
	background: rgba(220, 38, 38, 0.06);
}

.al-cell {
	padding: 6px 12px;
	font-size: 0.8125rem;
	border-bottom: 1px solid rgba(0, 0, 0, 0.06);
	vertical-align: middle;
	overflow: hidden;
	text-overflow: ellipsis;
	white-space: nowrap;
}

.al-row:last-child .al-cell {
	border-bottom: none;
}

/* ── Time cell ───────────────────────────────────────────── */
.al-cell-time {
	font-family: 'Roboto Mono', monospace;
	font-size: 0.75rem;
	color: rgba(0, 0, 0, 0.45);
}

/* ── Caption cell ────────────────────────────────────────── */
.al-caption-cell {
	display: flex;
	flex-direction: column;
	gap: 2px;
}

.al-caption-text {
	font-weight: 500;
	font-size: 0.875rem;
	color: rgba(0, 0, 0, 0.87);
}

.al-sub-text {
	font-size: 0.75rem;
	color: rgba(0, 0, 0, 0.45);
	font-style: italic;
}

/* ── Context ─────────────────────────────────────────────── */
.al-cell-context {
	font-size: 0.8125rem;
	color: rgba(0, 0, 0, 0.55);
}

/* ── Active column ───────────────────────────────────────── */
.al-cell-active {
	text-align: center;
}

/* ── Severity badge ──────────────────────────────────────── */
.al-badge {
	display: inline-flex;
	align-items: center;
	padding: 1px 8px;
	border-radius: 9999px;
	font-size: 0.7rem;
	font-weight: 600;
	border: 2px solid currentColor;
	text-transform: capitalize;
	white-space: nowrap;
}

.al-badge--red    { color: #dc2626; }
.al-badge--yellow { color: #ca8a04; }
.al-badge--blue   { color: #2563eb; }
.al-badge--green  { color: #16a34a; }
.al-badge--slate  { color: rgba(0, 0, 0, 0.45); }

/* ── Empty state ─────────────────────────────────────────── */
.al-cell-empty {
	padding: 0;
	border-bottom: none;
}

.al-empty-state {
	display: flex;
	flex-direction: column;
	align-items: center;
	gap: 6px;
	padding: 40px 16px;
}

.al-empty-title {
	font-size: 0.875rem;
	color: rgba(0, 0, 0, 0.38);
	font-style: italic;
}
</style>
