(() => {
"use strict";
const $ = id => document.getElementById(id);
const FEED_URL = "./api/network-status.json";
const allowedStates = new Set(["online","degraded","outage","maintenance","unknown"]);
const allowedNodeStates = new Set(["online","unstable","suspected_down","down","maintenance","recovering"]);
let timer = null;
const setText = (id,value) => { $(id).textContent = String(value); };
function formatTime(value) { const d=new Date(value); return Number.isNaN(d.getTime())?"Time unavailable":d.toLocaleString([],{dateStyle:"medium",timeStyle:"short"}); }
function safeText(value,max=120) { return typeof value==="string"?value.slice(0,max):""; }
function empty(title,description) { const w=document.createElement("div");w.className="empty-state";const s=document.createElement("strong");s.textContent=title;const p=document.createElement("p");p.textContent=description;w.append(s,p);return w; }
function feedState(connected,label) { $("feed-dot").classList.toggle("active",connected);setText("feed-label",label); }
function unknown(message) {
$("network-banner").dataset.state="unknown";setText("network-state-title","Awaiting network data");setText("network-state-description",message);setText("last-updated","NOT UPDATED");
["online-count","unstable-count","outage-count","maintenance-count"].forEach(id=>setText(id,"—"));
$("incident-list").replaceChildren(empty("No incident feed yet","Outages will appear here when the authenticated status service is available."));
$("node-list").replaceChildren(empty("Waiting for node reports","No node availability data has been received."));feedState(false,"Status feed unavailable");
}
function renderIncidents(items) {
const list=$("incident-list");list.replaceChildren();if(!items.length){list.append(empty("No active incidents","The status feed reports no active outages."));return;}
for(const incident of items.slice(0,30)){const item=document.createElement("article");item.className="incident-item";const top=document.createElement("div");top.className="incident-top";const title=document.createElement("strong");title.textContent=safeText(incident.title,100)||"Network incident";const state=document.createElement("span");state.className="state-tag "+(incident.status==="resolved"?"state-good":"state-warn");state.textContent=(safeText(incident.status,24)||"reported").replaceAll("_"," ").toUpperCase();top.append(title,state);const detail=document.createElement("p");detail.textContent=safeText(incident.summary,240)||"No additional details provided.";const meta=document.createElement("small");meta.textContent=(incident.startedAt?formatTime(incident.startedAt):"Start time unavailable")+(incident.resolvedAt?" · Resolved "+formatTime(incident.resolvedAt):"");item.append(top,detail,meta);list.append(item);}
}
function renderNodes(items) {
const list=$("node-list");list.replaceChildren();if(!items.length){list.append(empty("No nodes reported","The network has not supplied any public node-health records."));return;}
for(const node of items.slice(0,100)){const row=document.createElement("div");row.className="node-item";const info=document.createElement("div");const name=document.createElement("strong");name.textContent=safeText(node.label,48)||"Unnamed node";const id=document.createElement("small");id.textContent=safeText(node.publicId,32)||"Public identifier unavailable";info.append(name,id);const status=document.createElement("span");status.className="state-tag "+(node.state==="online"?"state-good":node.state==="maintenance"?"state-info":"state-warn");status.textContent=safeText(node.state,24).replaceAll("_"," ").toUpperCase();row.append(info,status);list.append(row);}
}
function valid(data) {
if(!data||data.schemaVersion!==1||!allowedStates.has(data.networkState)||!Array.isArray(data.nodes)||!Array.isArray(data.incidents)||typeof data.generatedAt!=="string")return false;
const generated=Date.parse(data.generatedAt);if(!Number.isFinite(generated)||generated>Date.now()+300000)return false;
return data.nodes.every(n=>n&&typeof n.publicId==="string"&&allowedNodeStates.has(n.state))&&data.incidents.every(i=>i&&typeof i.title==="string"&&typeof i.status==="string");
}
function render(data) {
const counts={online:0,unstable:0,maintenance:0};for(const n of data.nodes){if(n.state==="online")counts.online++;else if(n.state==="unstable"||n.state==="recovering")counts.unstable++;else if(n.state==="maintenance")counts.maintenance++;}
const active=data.incidents.filter(i=>i.status!=="resolved");setText("online-count",counts.online);setText("unstable-count",counts.unstable);setText("outage-count",active.length);setText("maintenance-count",counts.maintenance);
$("network-banner").dataset.state=data.networkState;
const labels={online:["Network operational","No active network-wide interruption is currently reported."],degraded:["Network degraded","Some nodes or links are experiencing reduced availability."],outage:["Network outage reported","One or more active service interruptions are reported."],maintenance:["Scheduled maintenance","Some network components are intentionally unavailable."],unknown:["Network status unknown","The status service cannot currently determine network availability."]};
setText("network-state-title",labels[data.networkState][0]);setText("network-state-description",safeText(data.summary,240)||labels[data.networkState][1]);setText("last-updated","UPDATED "+formatTime(data.generatedAt).toUpperCase());renderIncidents(data.incidents);renderNodes(data.nodes);
const fresh=Date.now()-Date.parse(data.generatedAt)<=120000;feedState(fresh,fresh?"Live status feed":"Status feed delayed");
}
async function refresh(){try{const response=await fetch(FEED_URL,{cache:"no-store",headers:{"Accept":"application/json"}});if(!response.ok)throw new Error(response.status===404?"The authenticated network status feed has not been configured yet.":"Status service returned HTTP "+response.status+".");const data=await response.json();if(!valid(data))throw new Error("Status feed format is invalid or unsupported.");render(data);}catch(error){unknown(safeText(error&&error.message,240)||"Status feed could not be reached.");}}
function start(){refresh();timer=window.setInterval(refresh,30000);}
window.addEventListener("online",refresh);window.addEventListener("beforeunload",()=>{if(timer!==null)window.clearInterval(timer);});start();
})();