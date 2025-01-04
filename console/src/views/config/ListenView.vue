<script lang="ts" setup>
import { onMounted, ref } from "vue";
const configListen = ref(<any[]>[])
const newListenForm = ref(false)
const newListen = ref({ ip: "", port: 0 })

const listens = ref(<any[]>[])

function updateConfigListen() {
    fetch('/core.whm?whm_call=get_config_listen&format=json').then((res) => {
        return res.json();
    }).then((json) => { configListen.value = json.result.listen; });

    fetch('/core.whm?whm_call=get_listen&format=json').then((res) => {
        return res.json();
    }).then((json) => { listens.value = json.result.listen; });
}
onMounted(() => {
    updateConfigListen();
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
</template>