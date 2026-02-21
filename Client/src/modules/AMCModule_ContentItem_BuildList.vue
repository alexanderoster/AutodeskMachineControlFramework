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
<div v-if="moduleitem.type === 'buildlist'" class="bl-root">

	<div class="bl-area">

		<!-- Sticky header -->
		<div class="bl-header">
			<table class="bl-table">
				<colgroup>
					<col style="width: 96px" />
					<col />
					<col style="width: 80px" />
					<col style="width: 160px" />
					<col style="width: 200px" />
					<col style="width: 110px" />
					<col v-if="hasActions" style="width: 1px" />
				</colgroup>
				<thead>
					<tr>
						<th></th>
						<th>Build</th>
						<th>Layers</th>
						<th>Uploaded</th>
						<th>UUID</th>
						<th>Status</th>
						<th v-if="hasActions"></th>
					</tr>
				</thead>
			</table>
		</div>

		<!-- Scrollable body -->
		<div class="bl-scroll">
			<table class="bl-table">
				<colgroup>
					<col style="width: 96px" />
					<col />
					<col style="width: 80px" />
					<col style="width: 160px" />
					<col style="width: 200px" />
					<col style="width: 110px" />
					<col v-if="hasActions" style="width: 1px" />
				</colgroup>
				<tbody>
					<!-- Empty state -->
					<tr v-if="!moduleitem.entries || moduleitem.entries.length === 0">
						<td :colspan="hasActions ? 7 : 6" class="bl-cell-empty">
							<div class="bl-empty-state">
								<v-icon color="grey lighten-1" size="40">mdi-folder-open-outline</v-icon>
								<span class="bl-empty-title">No build files uploaded</span>
								<span class="bl-empty-hint">Click a row to view details and select a build</span>
							</div>
						</td>
					</tr>

					<!-- Data rows -->
					<tr
						v-for="item in moduleitem.entries"
						:key="item.buildUUID"
						class="bl-row"
						@click="onRowClick(item)"
					>
						<!-- Thumbnail -->
						<td class="bl-cell bl-cell-thumb">
							<img
								v-if="hasThumbnail(item)"
								class="bl-thumb-img"
								:src="Application.getImageURL(item.buildThumbnail)"
							/>
							<div v-else class="bl-thumb-placeholder">
								<v-icon color="grey lighten-1" size="28">mdi-file-3d-outline</v-icon>
							</div>
						</td>

						<!-- Build name + execution count -->
						<td class="bl-cell">
							<div class="bl-name-cell">
								<span class="bl-name-text">{{ item.buildName }}</span>
								<span class="bl-sub-text">
									{{ item.buildExecutionCount === 1 ? 'Printed 1 time' : 'Printed ' + item.buildExecutionCount + ' times' }}
								</span>
							</div>
						</td>

						<!-- Layers -->
						<td class="bl-cell bl-cell-meta">{{ item.buildLayers }}</td>

						<!-- Upload time + user -->
						<td class="bl-cell">
							<div class="bl-date-cell">
								<span class="bl-date-text">{{ formatDateTime(item.buildTimestamp) }}</span>
								<span class="bl-sub-text">{{ item.buildUser }}</span>
							</div>
						</td>

						<!-- UUID -->
						<td class="bl-cell bl-cell-uuid">{{ item.buildUUID }}</td>

						<!-- Status -->
						<td class="bl-cell">
							<div class="bl-status-cell">
								<v-icon small color="green darken-1">mdi-check-circle</v-icon>
								<span class="bl-status-ready">Ready</span>
							</div>
						</td>

						<!-- Action buttons -->
						<td v-if="hasActions" class="bl-cell bl-cell-actions" @click.stop>
							<v-btn
								v-for="button in moduleitem.entrybuttons"
								:key="button.uuid"
								text
								x-small
								color="primary"
								@click.stop="uiModuleBuildListHistoryClick(button, item)"
							>
								<v-icon v-if="button.icon" small left>{{ button.icon }}</v-icon>
								{{ button.caption }}
							</v-btn>
						</td>
					</tr>
				</tbody>
			</table>
		</div>

	</div>

	<!-- Detail dialog -->
	<v-dialog v-model="showDialog" max-width="540" @click:outside="showDialog = false">
		<v-card v-if="selectedEntry">
			<v-card-title class="bl-dialog-title">
				<v-icon left color="primary">mdi-file-3d-outline</v-icon>
				Build details
				<v-spacer />
				<v-btn icon small @click="showDialog = false">
					<v-icon>mdi-close</v-icon>
				</v-btn>
			</v-card-title>
			<v-divider />

			<v-card-text class="bl-dialog-body pt-4">

				<div v-if="hasThumbnail(selectedEntry)" class="bl-dialog-thumb-row mb-4">
					<v-img
						:src="Application.getImageURL(selectedEntry.buildThumbnail)"
						max-height="160"
						contain
						class="rounded"
					/>
				</div>

				<div class="bl-dialog-row">
					<span class="bl-dialog-label">File name</span>
					<span class="bl-dialog-value font-weight-medium">{{ selectedEntry.buildName }}</span>
				</div>
				<div class="bl-dialog-row">
					<span class="bl-dialog-label">Layers</span>
					<span class="bl-dialog-value">{{ selectedEntry.buildLayers }}</span>
				</div>
				<div class="bl-dialog-row">
					<span class="bl-dialog-label">Uploaded by</span>
					<span class="bl-dialog-value">{{ selectedEntry.buildUser }}</span>
				</div>
				<div class="bl-dialog-row">
					<span class="bl-dialog-label">Upload time</span>
					<span class="bl-dialog-value">{{ formatDateTime(selectedEntry.buildTimestamp) }}</span>
				</div>
				<div class="bl-dialog-row">
					<span class="bl-dialog-label">Executions</span>
					<span class="bl-dialog-value">{{ selectedEntry.buildExecutionCount }}</span>
				</div>
				<div class="bl-dialog-row bl-dialog-uuid-row">
					<span class="bl-dialog-label">UUID</span>
					<span class="bl-uuid-text">{{ selectedEntry.buildUUID }}</span>
				</div>

			</v-card-text>

			<v-divider />
			<v-card-actions>
				<v-spacer />
				<v-btn text @click="showDialog = false">Cancel</v-btn>
				<v-btn color="primary" @click="onSelectFromDialog">
					<v-icon left small>mdi-check</v-icon>
					Select for execution
				</v-btn>
			</v-card-actions>
		</v-card>
	</v-dialog>

</div>
</template>

<script>
export default {
	props: ['Application', 'moduleitem'],

	data() {
		return {
			showDialog: false,
			selectedEntry: null,
		};
	},

	computed: {
		hasActions() {
			return this.moduleitem.entrybuttons && this.moduleitem.entrybuttons.length > 0;
		},
	},

	methods: {
		hasThumbnail(item) {
			return item.buildThumbnail &&
				item.buildThumbnail !== '00000000-0000-0000-0000-000000000000';
		},

		formatDateTime(timeString) {
			if (!timeString) return '';
			const date = new Date(timeString);
			if (isNaN(date.getTime())) return timeString;
			return new Intl.DateTimeFormat('en-US', {
				year: 'numeric',
				month: 'short',
				day: 'numeric',
				hour: '2-digit',
				minute: '2-digit',
				hour12: false,
			}).format(date);
		},

		onRowClick(item) {
			this.selectedEntry = item;
			this.showDialog = true;
		},

		onSelectFromDialog() {
			this.showDialog = false;
			if (this.selectedEntry && this.moduleitem.selectevent && this.moduleitem.selectionvalueuuid) {
				const eventValues = {};
				eventValues[this.moduleitem.selectionvalueuuid] = this.selectedEntry.buildUUID;
				this.Application.triggerUIEvent(this.moduleitem.selectevent, this.moduleitem.uuid, eventValues);
			}
		},

		uiModuleBuildListHistoryClick(button, item) {
			if (item && button && button.selectevent &&
				this.moduleitem.selectionvalueuuid && this.moduleitem.buttonvalueuuid) {
				const eventValues = {};
				eventValues[this.moduleitem.selectionvalueuuid] = item.buildUUID;
				eventValues[this.moduleitem.buttonvalueuuid] = button.uuid;
				this.Application.triggerUIEvent(button.selectevent, this.moduleitem.uuid, eventValues);
			}
		},
	},
};
</script>

<style scoped>
/* ── Root layout ─────────────────────────────────────────── */
.bl-root {
	width: 100%;
}

.bl-area {
	width: 100%;
	display: flex;
	flex-direction: column;
}

/* ── Header ──────────────────────────────────────────────── */
.bl-header {
	border: 1px solid rgba(0, 0, 0, 0.1);
	border-bottom: none;
	border-radius: 6px 6px 0 0;
	overflow: hidden;
}

/* ── Scrollable body ─────────────────────────────────────── */
.bl-scroll {
	overflow-x: hidden;
	border: 1px solid rgba(0, 0, 0, 0.1);
	border-top: none;
	border-radius: 0 0 6px 6px;
}

/* ── Shared table styles ─────────────────────────────────── */
.bl-table {
	width: 100%;
	border-collapse: collapse;
	table-layout: fixed;
}

.bl-table thead th {
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
.bl-row {
	cursor: pointer;
}

.bl-row:hover {
	background: rgba(0, 0, 0, 0.03);
}

.bl-cell {
	padding: 6px 12px;
	font-size: 0.875rem;
	border-bottom: 1px solid rgba(0, 0, 0, 0.06);
	vertical-align: middle;
	overflow: hidden;
	text-overflow: ellipsis;
	white-space: nowrap;
}

.bl-row:last-child .bl-cell {
	border-bottom: none;
}

/* ── Thumbnail cell ──────────────────────────────────────── */
.bl-cell-thumb {
	padding: 6px 12px;
}

.bl-thumb-img {
	display: block;
	width: 72px;
	height: 52px;
	object-fit: contain;
	border-radius: 4px;
	background: #f5f5f5;
}

.bl-thumb-placeholder {
	width: 72px;
	height: 52px;
	display: flex;
	align-items: center;
	justify-content: center;
	background: #f5f5f5;
	border-radius: 4px;
}

/* ── Name cell ───────────────────────────────────────────── */
.bl-name-cell {
	display: flex;
	flex-direction: column;
	gap: 2px;
}

.bl-name-text {
	font-weight: 500;
	font-size: 0.875rem;
	white-space: nowrap;
	overflow: hidden;
	text-overflow: ellipsis;
}

.bl-sub-text {
	font-size: 0.75rem;
	color: rgba(0, 0, 0, 0.45);
	font-style: italic;
}

/* ── Date cell ───────────────────────────────────────────── */
.bl-date-cell {
	display: flex;
	flex-direction: column;
	gap: 2px;
}

.bl-date-text {
	font-size: 0.8125rem;
}

/* ── Meta (layers) ───────────────────────────────────────── */
.bl-cell-meta {
	font-size: 0.8125rem;
	color: rgba(0, 0, 0, 0.55);
}

/* ── UUID ────────────────────────────────────────────────── */
.bl-cell-uuid {
	font-size: 0.7rem;
	font-family: 'Roboto Mono', monospace;
	color: rgba(0, 0, 0, 0.38);
	letter-spacing: 0.01em;
}

.bl-uuid-text {
	font-size: 0.7rem;
	font-family: 'Roboto Mono', monospace;
	color: rgba(0, 0, 0, 0.38);
	letter-spacing: 0.01em;
	word-break: break-all;
}

/* ── Status ──────────────────────────────────────────────── */
.bl-status-cell {
	display: flex;
	align-items: center;
	gap: 4px;
}

.bl-status-ready {
	font-size: 0.8125rem;
	color: #2e7d32;
	font-weight: 500;
}

/* ── Actions ─────────────────────────────────────────────── */
.bl-cell-actions {
	white-space: nowrap;
}

/* ── Empty state ─────────────────────────────────────────── */
.bl-cell-empty {
	padding: 0;
	border-bottom: none;
}

.bl-empty-state {
	display: flex;
	flex-direction: column;
	align-items: center;
	gap: 6px;
	padding: 48px 16px;
}

.bl-empty-title {
	font-size: 0.9375rem;
	color: rgba(0, 0, 0, 0.45);
}

.bl-empty-hint {
	font-size: 0.8125rem;
	color: rgba(0, 0, 0, 0.3);
	font-style: italic;
}

/* ── Detail dialog ───────────────────────────────────────── */
.bl-dialog-title {
	font-size: 1rem !important;
	padding: 14px 20px !important;
}

.bl-dialog-body {
	padding: 16px 20px !important;
}

.bl-dialog-thumb-row {
	border-radius: 6px;
	overflow: hidden;
	background: #f5f5f5;
}

.bl-dialog-row {
	display: flex;
	align-items: baseline;
	gap: 12px;
	padding: 5px 0;
	border-bottom: 1px solid rgba(0, 0, 0, 0.06);
}

.bl-dialog-row:last-child {
	border-bottom: none;
}

.bl-dialog-label {
	font-size: 0.8125rem;
	font-weight: 500;
	color: rgba(0, 0, 0, 0.55);
	min-width: 140px;
	flex-shrink: 0;
}

.bl-dialog-value {
	font-size: 0.875rem;
}

.bl-dialog-uuid-row .bl-uuid-text {
	word-break: break-all;
}
</style>
