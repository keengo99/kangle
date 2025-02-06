<script lang="ts" setup>
import { whm_get, whm_post } from "@/components/Whm.vue";
import { onMounted, ref } from "vue";
const configListen = ref(<any[]>[])
const newListenForm = ref(false)
const newListen = ref({ ip: "", port: 0 })

const listens = ref(<any[]>[])
interface ConfigItem {
    connect_time_out:number,
    time_out:number,
    worker_thread:number,
}
const configItem = ref<ConfigItem|null>(null);

function updateConfigListen() {
    fetch('/core.whm?whm_call=get_config_listen&format=json').then((res) => {
        return res.json();
    }).then((json) => { configListen.value = json.result.listen; });

    fetch('/core.whm?whm_call=get_listen&format=json').then((res) => {
        return res.json();
    }).then((json) => { listens.value = json.result.listen; });
}
function flushConfig() {
    whm_get('config',{item:0}).then((json) => {
        configItem.value = json.result;
     });
}
function submitConfig() {
    const form: any = document.getElementById("form");
    const formData = new FormData(form);
    formData.append('item','0');
    whm_post('config_submit',formData).then((json) => {
       flushConfig();
    });
}
onMounted(() => {
    updateConfigListen();
    flushConfig();
});
function onNewListenForm() {
    newListenForm.value = !newListenForm.value;
}
function onNewListenSubmit() {
    updateConfigListen();
}
</script>
<template>
    <table>
        <tr>
            <td>
                配置侦听
                <table border=1 cellspacing="0">
                    <tr>
                        <td>操作</td>
                        <td>ip</td>
                        <td>port</td>
                        <td>服务</td>
                    </tr>
                    <tr v-for="listen in configListen">
                        <td>-</td>
                        <td>{{ listen.ip }}</td>
                        <td>{{ listen.port }}</td>
                        <td>{{ listen.type }}</td>
                    </tr>
                </table>
            </td>
            <td>
                成功侦听
                <table border=1 cellspacing="0">
                    <tr>                       
                        <td>ip</td>
                        <td>port</td>
                        <td>服务</td>
                        <td>协议</td>
                    </tr>
                    <tr v-for="listen in listens">
                        <td>{{ listen.ip }}</td>
                        <td>{{ listen.port }}</td>
                        <td>{{ listen.type }}</td>
                        <td>{{ listen.tcp_ip }}</td>
                    </tr>
                </table>
            </td>
        </tr>
    </table>
    <div>
        <input type="button" @click="onNewListenForm" value="新增" />
        <div v-if="newListenForm">
            ip:<input type="text" v-model="newListen.ip">
            port:<input type="text" v-model="newListen.port">
            <input type="button" @click="onNewListenSubmit" value="确定">
        </div>
    </div>
    <div v-if="configItem">
        <form onsubmit="return false;" id="form">
        <div>连接超时: <input name='connect_time_out' :value="configItem.connect_time_out"></div>
        <div>超时: <input name="time_out" :value="configItem.time_out"></div>
        <div>工作线程:<input name="worker_thread" :value="configItem.worker_thread"></div>
        <input type="button" @click="submitConfig" value="提交">
        </form>
    </div>
</template>