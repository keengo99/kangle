<script lang="ts" setup>
import { whm_get } from '@/components/Whm.vue';
import { onMounted, ref } from 'vue';
interface Sserver {
    name: string,
    host: string,
    port: number,
    proto: string,
}
const sservers = ref(<Sserver[]>[])
function flushSserver() {
    whm_get("list_sserver").then((json) => {
        sservers.value = json.result.sserver;
    });
}
onMounted(flushSserver)
</script>
<template>
    [刷新] [新增]
    <table>
        <tr>
            <td>操作</td>
            <td>名称</td>
            <td>地址</td>
            <td>端口</td>
            <td>协议</td>
        </tr>
        <tr v-for="server in sservers">
            <td>[删除]</td>
            <td>{{ server.name }}</td>
            <td>{{ server.host }}</td>
            <td>{{ server.port }}</td>
            <td>{{ server.proto }}</td>
        </tr>
    </table>
</template>