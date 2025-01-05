<script lang="ts" setup>
import { onMounted, ref } from 'vue';
const connections = ref(<any[]>[]);
function flushConnect() {
    fetch('/core.whm?whm_call=connection&format=json').then((res) => res.json()).then((json) => {
        connections.value = json.result.c;
    });
}
onMounted(flushConnect);
</script>
<template>
    [<a href=# @click="flushConnect()">刷新</a>]
    <table border="1" cellspacing="0">
        <tr>
            <th>源地址</th>
            <th>时间</th>
            <th>状态</th>
            <th>方法</th>
            <th>URL</th>
            <th>referer</th>
            <th>HTTP</th>
        </tr>
        <tr v-for="c in connections">
            <td>{{ c.src }}</td>
            <td>{{ c.time }}</td>
            <td>{{ c.state }}</td>
            <td>{{ c.meth }}</td>
            <td>{{ c.url }}</td>
            <td>{{ c.referer }}</td>
            <td>{{ c.version }}</td>
        </tr>
    </table>
</template>