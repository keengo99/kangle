<script lang="ts" setup>
import FooterView from "./FooterView.vue";
import { whm_get, whm_post } from '@/components/Whm.vue';
import { onMounted, ref } from 'vue';
interface Process {
    app: string,
    pid: number,
    cpu_usage: number,
    low_pri: number,
    refs: number,
    size: number,
    total_run: number,
}
interface ProcessGroup {
    name: string,
    process: Process[]
}

const processGroups = ref(<ProcessGroup[]>[])
function flushProcess() {
    whm_get("list_process").then((json) => {
        processGroups.value = json.result.group;
    });
}
function kill_process(name: string, process: Process) {
    if (!confirm("确定关闭进程 " + process.pid + "吗?")) {
        return;
    }
    whm_post("kill_process", { name: name, app: process.app, pid: process.pid.toString() }).then((json) => {
        flushProcess();
    })
}
onMounted(flushProcess)
</script>
<template>
    <div>子进程信息 [<a href=# @click="flushProcess()">刷新</a>]</div>
    <template v-for="group in processGroups">
        {{ group.name }} ({{ group.process.length }})
        <table border="1" cellspacing="0">
            <tr>
                <th>操作</th>
                <th>用户名</th>
                <th>pid</th>
                <th>使用数</th>
                <th>空闲数</th>
                <th>运行时间</th>
            </tr>
            <tr v-for="process in group.process">
                <td><a href=# @click="kill_process(group.name, process)">关闭</a></td>
                <td>{{ process.app }}</td>
                <td>{{ process.pid }}</td>
                <td>{{ process.refs }}</td>
                <td>{{ process.size }}</td>
                <td>{{ process.total_run }}</td>
            </tr>
        </table>
    </template>
    <FooterView />
</template>