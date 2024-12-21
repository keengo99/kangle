<script lang="ts" setup>
import { onMounted, ref } from 'vue';
import AccessView from './acccess/AccessView.vue'
import FooterView from '@/views/FooterView.vue';
const virtualHosts = ref(<any[]>[])
const vh_infos = ref(<any[]>[])
const vh_indexs = ref(<any[]>[])
const curVirtualHost = ref(<string | null>null)
const curVhTab = ref("")
function get_whm_url(call_name :string,vh:string|null) :string {
    let url = '/core.whm?whm_call='+call_name+'&format=json'
    if (vh && vh.length>0) {
        url += '&name='+vh
    }
    return url;
}
function flushVirtualHost() {
    fetch(get_whm_url('get_vh','')).then((res) => res.json()).then((json) => {
        virtualHosts.value = json.result.vh;
    });
}
function addVirtualHost() {

}
function delVirtualHost(vh: any) {
    if (!confirm("确定要删除 " + vh.name + "?")) {
        return;
    }
    fetch(get_whm_url('vh_action',vh.name)+'&action=vh_delete').then((res) => res.json()).then((json) => {
        flushVirtualHost();
    });
}
function gotoAllViralhost() {
    curVirtualHost.value = null;
}
function setCurVirtualHost(vh: any) {
    curVirtualHost.value = vh;
}
function switchTab(tab:string) {
    switch(tab) {
        case 'info':
        fetch(get_whm_url('get_vh_info',curVirtualHost.value)).then((res) => res.json()).then((json) => {
            vh_infos.value = json.result;
            curVhTab.value = tab;
        });
        return;
        case 'index':
        fetch(get_whm_url('get_vh_index',curVirtualHost.value)).then((res) => res.json()).then((json) => {
            vh_indexs.value = json.result;
            curVhTab.value = tab;
        });
        return;
    }
    curVhTab.value = tab;
}
onMounted(flushVirtualHost);
</script>
<template>    
    <div v-if="curVirtualHost == null">
        [<a href=# @click='flushVirtualHost'>刷新</a>]
        [<a href=# @click='addVirtualHost'>新增</a>]        
        [<a href=# @click='setCurVirtualHost("")'>全局设置</a>]
        <hr>
        <table width="100%">
            <tr>
                <td>操作</td>
                <td>名称</td>
                <td>主目录</td>
                <td>控制文件</td>
                <td>连接限制</td>
            </tr>
            <tr v-for="vh in virtualHosts">
                <td>[<a href=# @click='delVirtualHost(vh)'>删除</a>]</td>
                <td><a href=# @click="setCurVirtualHost(vh.name)">{{ vh.name }}</a></td>
                <td>{{ vh.doc_root }}</td>
                <td>{{ vh.access }}</td>
                <td>{{ vh.max_connect }}</td>
            </tr>
        </table>
    </div>
    <div v-else>        
        [<a href=# @click="gotoAllViralhost()">虚拟主机</a>] ==> 
        <span v-if="curVirtualHost.length>0">
        {{ curVirtualHost }}
        </span>
        <span v-else>全局设置</span>
        <hr>
        <table width='100%'>
            <tr>
                <td>
                    <template v-if="curVirtualHost.length>0">
                    [<a href="#" @click="switchTab('info')">详细</a>]
                    </template>
                    [<a href="#" @click="switchTab('index')">默认文件</a>]
                    [<a href="#" @click="switchTab('extend')">扩展映射</a>]
                    [<a href="#" @click="switchTab('error')">自定义错误页面</a>]
                    [<a href="#" @click="switchTab('alias')">别名</a>]
                    [<a href="#" @click="switchTab('mime')">mime类型</a>]
                    [<a href="#" @click="switchTab('host')">主机头</a>]
                    [<a href="#" @click="switchTab('bind')">绑定</a>]
                    <template v-if="curVirtualHost.length>0">
                    [<a href="#" @click="switchTab('request')">请求控制</a>]
                    [<a href="#" @click="switchTab('response')">回应控制</a>]
                    </template>
                </td>
            </tr>
        </table>
        
        <div v-if="curVhTab=='request'">
            <AccessView access="request" :vh="curVirtualHost" ></AccessView>
        </div>
        <div v-if="curVhTab=='response'">
            <AccessView access="response" :vh="curVirtualHost" ></AccessView>
        </div>        
    </div>
    <FooterView/>
</template>
