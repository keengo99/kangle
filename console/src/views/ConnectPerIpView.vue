<script lang="ts" setup>
import FooterView from "./FooterView.vue";
import { whm_get, whm_post } from '@/components/Whm.vue';
import { onMounted, ref } from 'vue';
interface ConnectPerIp {
    ip: string,
    count: number,
}

const connectPerIps = ref(<ConnectPerIp[]>[])
const maxPerIp = ref(0);
function flushConnectPerIp() {
    whm_get("list_connect_per_ip").then((json) => {
            connectPerIps.value = json.result.info;
            maxPerIp.value = json.result.max_per_ip;
    });
}
onMounted(flushConnectPerIp)
</script>
<template>
      每IP连接信息({{ maxPerIp }}) [<a href="#" @click="flushConnectPerIp()">刷新</a>]
      <table border="1" cellspacing="0">
            <tr>
                <th>ip地址</th>
                <th>连接数</th>
            </tr>
            <tr v-for="info in connectPerIps">
                <td>{{ info.ip }}</td>
                <td>{{ info.count }}</td>
            </tr>
        </table>
      <FooterView />
</template>