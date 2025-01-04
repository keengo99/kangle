<script lang="ts" setup>
import type { ServerNode } from '@/components/ServerNode.vue';
import { whm_get } from '@/components/Whm.vue';
import { onMounted, ref } from 'vue';

export interface MserverNode extends ServerNode{
    weight:number,
    enable:number,
    avg_monitor_tick:number,
}
export interface Mserver {
    name: string,
    proto: string,
    hash: string,
    cookie_stick:number,
    err_try_time:number,
    max_err_count:number,
    refs:number,
    icp:string,
    show_node:boolean,
    nodes : MserverNode[],
}
const new_server = ref(false);
const mservers = ref(<Mserver[]>[])
function flushMserver() {
    whm_get("list_mserver").then((json) => {
        mservers.value = json.result.mserver;
    });
}
function del_server(server: Mserver) {
    if (!confirm("确定删除 " + server.name + "吗?")) {
        return;
    }
    whm_get("del_sserver", { name: server.name }).then((json) => {
        flushMserver();
    })
}
function toggle_node(server: Mserver) {
    server.show_node=!server.show_node;
}
onMounted(flushMserver)
</script>
<template>
    <!--    <th>地址</th>
            <th>端口</th>
            <th>生存时间</th>
            <th>空闲数</th>
            <th>权重</th>
            <th>self</th>
            <th>up/hit</th>
            <th>err/avg</th>
            <th>状态</th>
            -->
    <table border=1 cellspacing="0">
        <tr>
            <th>操作</th>
            <th>名字</th>
            <th>协议</th>
            <th>hash</th>
            <th>cookie粘住</th>
            <th>错误重试(秒)</th>
            <th>连续错误次数</th>
            <th>引用</th>
            <th>icp</th>
            <th>节点</th>
            <th>节点信息</th>
        </tr>
        <tr v-for="server in mservers">

            <td>
                [<a href="#" @click="">删除</a>]
                [<a href='#'>修改</a>]
               
            </td>
            <td>{{ server.name }}</td>
            <td>{{ server.proto }}</td>
            <td>{{ server.hash }}</td>
            <td>{{ server.cookie_stick }}</td>
            <td>{{ server.err_try_time }}</td>
            <td>{{ server.max_err_count }}</td>
            <td>{{ server.refs }}</td>
            <td>{{ server.icp }}</td>
            <td >
                <span>{{ server.nodes.length }}</span>
                <span >                    
                    [<a href='#'>增加</a>]
                    [<a href='#' @click="toggle_node(server)">详细</a>]
                </span>
            </td>
            <td>
                <div v-if="server.show_node">
                    <table width="100%" border=1 cellspacing="0">
                        <tr>
                            <th>地址</th>
                            <th>端口</th>
                            <th>生存时间</th>
                            <th>空闲数</th>
                            <th>权重</th>
                            <th>self</th>
                            <th>up/hit</th>
                            <th>err/avg</th>
                            <th>状态</th>
                        </tr>
                        <tr v-for="(node, index) in server.nodes">                           
                            <td>[删][改]{{ node.host }}</td>
                            <td>{{ node.port }}</td>
                            <td>{{ node.life_time }}</td>
                            <td>{{ node.idle_size }}</td>
                            <td>{{ node.weight }}</td>
                            <td>{{ node.self_ip }}</td>
                            <td>{{ node.total_connect }}/{{ node.total_hit }}</td>
                            <td>{{ node.total_err }}/{{ node.avg_monitor_tick }}</td>
                            <td>{{ node.enable }}</td>
                        </tr>
                    </table>
                </div>
            </td>
        </tr>


    </table>
    <!--
    <template v-if="server.nodes.length == 0">
                <tr>
                    <MServerCommon :server="server"></MServerCommon>
                    <td colspan=9 :rowspan="server.nodes.length">&nbsp;</td>
                </tr>
            </template>
            <template v-else>
                <tr v-for="(node, index) in server.nodes">
                    <template v-if="index == 0">
                        <MServerCommon :server="server"></MServerCommon>
                    </template>
                    <td>[删][改]{{ node.host }}</td>
                    <td>{{ node.port }}</td>
                    <td>{{ node.life_time }}</td>
                    <td>{{ node.idle_size }}</td>
                    <td>{{ node.weight }}</td>
                    <td>{{ node.self_ip }}</td>
                    <td>{{ node.total_connect }}/{{ node.total_hit }}</td>
                    <td>{{ node.total_err }}/{{ node.avg_monitor_tick }}</td>
                    <td>{{ node.enable }}</td>
                </tr>
            </template>
            -->
</template>