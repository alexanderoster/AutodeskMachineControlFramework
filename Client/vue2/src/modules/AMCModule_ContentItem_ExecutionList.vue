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
<div v-if="moduleitem.type === 'executionlist'" class="el-root">

	<div class="el-area">

		<!-- Sticky header -->
		<div class="el-header">
			<table class="el-table">
				<colgroup>
					<col v-if="hasThumbnails" style="width: 112px" />
					<col />
					<col style="width: 180px" />
					<col style="width: 110px" />
					<col v-if="hasButtons" style="width: 160px" />
				</colgroup>
				<thead>
					<tr>
						<th v-if="hasThumbnails"></th>
						<th>Execution</th>
						<th>Time</th>
						<th>Status</th>
						<th v-if="hasButtons">Actions</th>
					</tr>
				</thead>
			</table>
		</div>

		<!-- Scrollable body -->
		<div class="el-scroll">
			<table class="el-table">
				<colgroup>
					<col v-if="hasThumbnails" style="width: 112px" />
					<col />
					<col style="width: 180px" />
					<col style="width: 110px" />
					<col v-if="hasButtons" style="width: 160px" />
				</colgroup>
				<tbody>

					<!-- Empty state -->
					<tr v-if="!moduleitem.entries || moduleitem.entries.length === 0">
						<td :colspan="columnCount" class="el-cell-empty">
							<div class="el-empty-state">
								<v-icon color="grey lighten-1" size="36">mdi-history</v-icon>
								<span class="el-empty-title">{{ moduleitem.loadingtext || 'No executions' }}</span>
								<span class="el-empty-hint">Build executions will appear here</span>
							</div>
						</td>
					</tr>

					<!-- Data rows -->
					<tr
						v-for="item in moduleitem.entries"
						:key="item.executionUUID"
						class="el-row"
						@click="onRowClick(item)"
					>
						<!-- Thumbnail -->
						<td v-if="hasThumbnails" class="el-cell el-cell-thumb">
							<img
								v-if="hasThumbnailImage(item)"
								:src="Application.getImageURL(item.executionThumbnail)"
								class="el-thumb-img"
								:alt="item.executionName"
								@click.stop="onRowClick(item)"
							/>
							<div v-else class="el-thumb-placeholder">
								<v-icon color="grey lighten-2" size="28">mdi-cube-outline</v-icon>
							</div>
						</td>

						<!-- Name + description -->
						<td class="el-cell el-name-cell">
							<div class="el-name-text">{{ item.executionName }}</div>
							<div v-if="item.executionDescription" class="el-sub-text">{{ item.executionDescription }}</div>
							<div v-if="item.executionLayerCount" class="el-sub-text">{{ item.executionLayerCount }} layers</div>
						</td>

						<!-- Timestamp block -->
						<td class="el-cell el-cell-time">
							<div v-if="item.executionStartTimestamp" class="el-time-row">
								<span class="el-time-label">Start</span>
								<span class="el-time-value">{{ formatDateTime(item.executionStartTimestamp) }}</span>
							</div>
							<div v-if="item.executionEndTimestamp" class="el-time-row">
								<span class="el-time-label">End</span>
								<span class="el-time-value">{{ formatDateTime(item.executionEndTimestamp) }}</span>
							</div>
							<div v-if="item.executionDuration != null" class="el-time-row">
								<span class="el-time-label">Dur</span>
								<span class="el-time-value el-duration">{{ formatDuration(item.executionDuration) }}</span>
							</div>
						</td>

						<!-- Status badge -->
						<td class="el-cell">
							<span v-if="item.executionStatus" class="el-status-badge" :class="statusClass(item.executionStatus)">
								{{ item.executionStatus }}
							</span>
						</td>

						<!-- Action buttons -->
						<td v-if="hasButtons" class="el-cell el-cell-actions" @click.stop>
							<button
								v-for="button in moduleitem.entrybuttons"
								:key="button.uuid"
								class="el-action-btn"
								@click.stop="onButtonClick(button, item)"
							>
								<v-icon v-if="button.icon" small>{{ button.icon }}</v-icon>
								<span v-if="button.caption">{{ button.caption }}</span>
							</button>
						</td>

					</tr>

				</tbody>
			</table>
		</div>

	</div>

	<!-- Detail dialog -->
	<v-dialog v-if="selectedItem" v-model="dialogOpen" max-width="520" @click:outside="closeDialog">
		<v-card class="el-dialog-card">
			<div class="el-dialog-title">Execution Details</div>

			<!-- Thumbnail row if available -->
			<div v-if="hasThumbnailImage(selectedItem)" class="el-dialog-thumb-row">
				<img
					:src="Application.getImageURL(selectedItem.executionThumbnail)"
					class="el-dialog-thumb-img"
					:alt="selectedItem.executionName"
				/>
			</div>

			<div class="el-dialog-body">
				<div class="el-dialog-row">
					<span class="el-dialog-label">Name</span>
					<span class="el-dialog-value">{{ selectedItem.executionName }}</span>
				</div>
				<div v-if="selectedItem.executionDescription" class="el-dialog-row">
					<span class="el-dialog-label">Description</span>
					<span class="el-dialog-value">{{ selectedItem.executionDescription }}</span>
				</div>
				<div v-if="selectedItem.executionStatus" class="el-dialog-row">
					<span class="el-dialog-label">Status</span>
					<span class="el-dialog-value">
						<span class="el-status-badge" :class="statusClass(selectedItem.executionStatus)">
							{{ selectedItem.executionStatus }}
						</span>
					</span>
				</div>
				<div v-if="selectedItem.executionLayerCount" class="el-dialog-row">
					<span class="el-dialog-label">Layers</span>
					<span class="el-dialog-value">{{ selectedItem.executionLayerCount }}</span>
				</div>
				<div v-if="selectedItem.executionStartTimestamp" class="el-dialog-row">
					<span class="el-dialog-label">Started</span>
					<span class="el-dialog-value">{{ formatDateTime(selectedItem.executionStartTimestamp) }}</span>
				</div>
				<div v-if="selectedItem.executionEndTimestamp" class="el-dialog-row">
					<span class="el-dialog-label">Finished</span>
					<span class="el-dialog-value">{{ formatDateTime(selectedItem.executionEndTimestamp) }}</span>
				</div>
				<div v-if="selectedItem.executionDuration != null" class="el-dialog-row">
					<span class="el-dialog-label">Duration</span>
					<span class="el-dialog-value">{{ formatDuration(selectedItem.executionDuration) }}</span>
				</div>
				<div v-if="selectedItem.executionUUID" class="el-dialog-row">
					<span class="el-dialog-label">UUID</span>
					<span class="el-dialog-value el-dialog-uuid">{{ selectedItem.executionUUID }}</span>
				</div>
			</div>

			<!-- Dialog buttons -->
			<div v-if="hasButtons" class="el-dialog-actions">
				<button
					v-for="button in moduleitem.entrybuttons"
					:key="button.uuid"
					class="el-dialog-action-btn"
					@click="onButtonClick(button, selectedItem); closeDialog()"
				>
					<v-icon v-if="button.icon" small>{{ button.icon }}</v-icon>
					{{ button.caption }}
				</button>
			</div>

			<div class="el-dialog-close">
				<button class="el-close-btn" @click="closeDialog">Close</button>
			</div>
		</v-card>
	</v-dialog>

</div>
</template>

<script>
const NULL_UUID = '00000000-0000-0000-0000-000000000000';

const STATUS_CLASSES = {
	running:    'el-status--running',
	active:     'el-status--running',
	finished:   'el-status--finished',
	completed:  'el-status--finished',
	succeeded:  'el-status--finished',
	failed:     'el-status--failed',
	error:      'el-status--failed',
	aborted:    'el-status--aborted',
	cancelled:  'el-status--aborted',
	pending:    'el-status--pending',
	queued:     'el-status--pending',
	paused:     'el-status--paused',
};

export default {
	props: ['Application', 'moduleitem'],

	data() {
		return {
			dialogOpen: false,
			selectedItem: null,
		};
	},

	computed: {
		hasThumbnails() {
			if (!this.moduleitem.entries || this.moduleitem.entries.length === 0) return false;
			return this.moduleitem.entries.some(e => this.hasThumbnailImage(e));
		},

		hasButtons() {
			return this.moduleitem.entrybuttons && this.moduleitem.entrybuttons.length > 0;
		},

		columnCount() {
			let cols = 3; // name + time + status
			if (this.hasThumbnails) cols++;
			if (this.hasButtons) cols++;
			return cols;
		},
	},

	methods: {
		hasThumbnailImage(item) {
			return item && item.executionThumbnail && item.executionThumbnail !== NULL_UUID;
		},

		statusClass(status) {
			return STATUS_CLASSES[(status || '').toLowerCase()] || 'el-status--pending';
		},

		formatDateTime(timeString) {
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

		formatDuration(durationInSeconds) {
			if (durationInSeconds == null) return '';
			const s = Math.round(durationInSeconds);
			const h = Math.floor(s / 3600);
			const m = Math.floor((s % 3600) / 60);
			const sec = s % 60;
			return [h, m, sec].map(n => String(n).padStart(2, '0')).join(':');
		},

		onRowClick(item) {
			this.selectedItem = item;
			this.dialogOpen = true;

			if (item && this.moduleitem.selectevent && this.moduleitem.selectionvalueuuid) {
				const eventValues = {};
				eventValues[this.moduleitem.selectionvalueuuid] = item.executionUUID;
				this.Application.triggerUIEvent(this.moduleitem.selectevent, this.moduleitem.uuid, eventValues);
			}
		},

		onButtonClick(button, item) {
			if (item && button && button.selectevent) {
				const eventValues = {};
				if (this.moduleitem.selectionvalueuuid) eventValues[this.moduleitem.selectionvalueuuid] = item.executionUUID;
				if (this.moduleitem.buttonvalueuuid)    eventValues[this.moduleitem.buttonvalueuuid]    = button.uuid;
				this.Application.triggerUIEvent(button.selectevent, this.moduleitem.uuid, eventValues);
			}
		},

		closeDialog() {
			this.dialogOpen = false;
			this.selectedItem = null;
		},
	},
};
</script>

<style scoped>
/* ── Root ────────────────────────────────────────────────── */
.el-root {
	width: 100%;
	display: flex;
	flex-direction: column;
}

.el-area {
	width: 100%;
	display: flex;
	flex-direction: column;
}

/* ── Header ──────────────────────────────────────────────── */
.el-header {
	border: 1px solid rgba(0, 0, 0, 0.1);
	border-bottom: none;
	border-radius: 6px 6px 0 0;
	overflow: hidden;
}

/* ── Scrollable body ─────────────────────────────────────── */
.el-scroll {
	overflow-x: hidden;
	border: 1px solid rgba(0, 0, 0, 0.1);
	border-top: none;
	border-radius: 0 0 6px 6px;
}

/* ── Shared table ────────────────────────────────────────── */
.el-table {
	width: 100%;
	border-collapse: collapse;
	table-layout: fixed;
}

.el-table thead th {
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
.el-row {
	cursor: pointer;
}

.el-row:hover {
	background: rgba(0, 0, 0, 0.03);
}

.el-cell {
	padding: 8px 12px;
	font-size: 0.8125rem;
	border-bottom: 1px solid rgba(0, 0, 0, 0.06);
	vertical-align: middle;
	overflow: hidden;
}

.el-row:last-child .el-cell {
	border-bottom: none;
}

/* ── Thumbnail ───────────────────────────────────────────── */
.el-cell-thumb {
	padding: 6px 8px;
}

.el-thumb-img {
	width: 96px;
	height: 72px;
	object-fit: contain;
	display: block;
	border-radius: 4px;
	background: #f5f5f5;
}

.el-thumb-placeholder {
	width: 96px;
	height: 72px;
	display: flex;
	align-items: center;
	justify-content: center;
	background: #f5f5f5;
	border-radius: 4px;
}

/* ── Name cell ───────────────────────────────────────────── */
.el-name-cell {
	overflow: hidden;
}

.el-name-text {
	font-weight: 500;
	font-size: 0.875rem;
	color: rgba(0, 0, 0, 0.87);
	white-space: nowrap;
	overflow: hidden;
	text-overflow: ellipsis;
}

.el-sub-text {
	font-size: 0.75rem;
	color: rgba(0, 0, 0, 0.45);
	margin-top: 2px;
	white-space: nowrap;
	overflow: hidden;
	text-overflow: ellipsis;
}

/* ── Time block ──────────────────────────────────────────── */
.el-cell-time {
	vertical-align: top;
	padding-top: 10px;
}

.el-time-row {
	display: flex;
	gap: 6px;
	align-items: baseline;
	line-height: 1.6;
}

.el-time-label {
	font-size: 0.7rem;
	font-weight: 600;
	color: rgba(0, 0, 0, 0.38);
	text-transform: uppercase;
	letter-spacing: 0.03em;
	min-width: 28px;
	flex-shrink: 0;
}

.el-time-value {
	font-family: 'Roboto Mono', monospace;
	font-size: 0.75rem;
	color: rgba(0, 0, 0, 0.6);
	white-space: nowrap;
}

.el-duration {
	color: rgba(0, 0, 0, 0.87);
	font-weight: 500;
}

/* ── Status badge ────────────────────────────────────────── */
.el-status-badge {
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

.el-status--running   { color: #2563eb; }
.el-status--finished  { color: #16a34a; }
.el-status--failed    { color: #dc2626; }
.el-status--aborted   { color: #ca8a04; }
.el-status--pending   { color: rgba(0, 0, 0, 0.45); }
.el-status--paused    { color: #7c3aed; }

/* ── Actions ─────────────────────────────────────────────── */
.el-cell-actions {
	white-space: nowrap;
}

.el-action-btn {
	display: inline-flex;
	align-items: center;
	gap: 4px;
	padding: 3px 10px;
	border: 1px solid rgba(0, 0, 0, 0.15);
	border-radius: 4px;
	background: transparent;
	cursor: pointer;
	font-size: 0.8125rem;
	font-weight: 500;
	color: rgba(0, 0, 0, 0.7);
	margin-right: 6px;
	transition: background 0.15s;
}

.el-action-btn:hover {
	background: rgba(0, 0, 0, 0.05);
	border-color: rgba(0, 0, 0, 0.3);
}

/* ── Empty state ─────────────────────────────────────────── */
.el-cell-empty {
	padding: 0;
	border-bottom: none;
}

.el-empty-state {
	display: flex;
	flex-direction: column;
	align-items: center;
	gap: 6px;
	padding: 48px 16px;
}

.el-empty-title {
	font-size: 0.9375rem;
	font-weight: 500;
	color: rgba(0, 0, 0, 0.55);
}

.el-empty-hint {
	font-size: 0.8125rem;
	color: rgba(0, 0, 0, 0.38);
}

/* ── Dialog ──────────────────────────────────────────────── */
.el-dialog-card {
	border-radius: 8px !important;
	box-shadow: 0 4px 24px rgba(0, 0, 0, 0.12) !important;
	overflow: hidden;
}

.el-dialog-title {
	font-size: 1rem;
	font-weight: 600;
	color: rgba(0, 0, 0, 0.87);
	padding: 20px 24px 0;
}

.el-dialog-thumb-row {
	padding: 16px 24px 0;
	display: flex;
	justify-content: center;
}

.el-dialog-thumb-img {
	max-width: 100%;
	max-height: 200px;
	object-fit: contain;
	border-radius: 6px;
	background: #f5f5f5;
}

.el-dialog-body {
	padding: 16px 24px;
	display: flex;
	flex-direction: column;
	gap: 10px;
}

.el-dialog-row {
	display: flex;
	gap: 12px;
	align-items: flex-start;
}

.el-dialog-label {
	font-size: 0.75rem;
	font-weight: 600;
	color: rgba(0, 0, 0, 0.45);
	text-transform: uppercase;
	letter-spacing: 0.03em;
	min-width: 72px;
	flex-shrink: 0;
	padding-top: 2px;
}

.el-dialog-value {
	font-size: 0.875rem;
	color: rgba(0, 0, 0, 0.87);
}

.el-dialog-uuid {
	font-family: 'Roboto Mono', monospace;
	font-size: 0.75rem;
	color: rgba(0, 0, 0, 0.45);
	word-break: break-all;
}

.el-dialog-actions {
	padding: 0 24px 8px;
	display: flex;
	gap: 8px;
	flex-wrap: wrap;
}

.el-dialog-action-btn {
	display: inline-flex;
	align-items: center;
	gap: 5px;
	padding: 6px 14px;
	border: 1px solid rgba(0, 0, 0, 0.15);
	border-radius: 4px;
	background: transparent;
	cursor: pointer;
	font-size: 0.875rem;
	font-weight: 500;
	color: rgba(0, 0, 0, 0.7);
	transition: background 0.15s;
}

.el-dialog-action-btn:hover {
	background: rgba(0, 0, 0, 0.05);
	border-color: rgba(0, 0, 0, 0.3);
}

.el-dialog-close {
	padding: 8px 24px 20px;
	display: flex;
	justify-content: flex-end;
}

.el-close-btn {
	padding: 6px 16px;
	border-radius: 4px;
	border: 1px solid rgba(0, 0, 0, 0.15);
	background: transparent;
	cursor: pointer;
	font-size: 0.875rem;
	font-weight: 500;
	color: rgba(0, 0, 0, 0.6);
	transition: background 0.15s;
}

.el-close-btn:hover {
	background: rgba(0, 0, 0, 0.05);
}
</style>
