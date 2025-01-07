<script lang="ts" setup>
import { onMounted, ref } from 'vue';
import TableView from './TableView.vue'
import DefaultAction from './ActionView.vue';
import { whm_get } from '@/components/Whm.vue';
import ModuleView, { type ModuleBase } from './ModuleView.vue';
import ModuleSelectView from './ModuleSelectView.vue';

const props = defineProps<{ access: string, vh?: string }>();
interface TableView {
    name: string,
    refs: number,
    show_chain?: boolean,
};
interface NamedModule extends ModuleBase {
    name: string
}
interface NamedModuleList {
    acl: NamedModule[],
    mark: NamedModule[]
}
interface EditNamedModule {
    m?: ModuleBase,
    name?: string,
    type: number,
    edit: boolean
}
const tables = ref(<TableView[] | null>null);
const named_modules = ref(<NamedModuleList | null>null);
function get_whm_url(call_name: string) {
    let url: string = '/core.whm?format=json&whm_call=' + call_name + '&access=' + props.access;
    if (props.vh) {
        url += '&vh=' + props.vh
    }
    return url
}
function flush_table() {
    fetch(get_whm_url('list_table')).then((res) => res.json()).then((json) => {
        tables.value = json.result.table;
        if (tables.value == null) {
            tables.value = [];
        }
    });
}
function flush_named_module() {
    var param: Record<string, any> = { access: props.access, detail: 1 };
    if (props.vh) {
        param["vh"] = props.vh;
    }
    whm_get('list_named_module', param).then((json) => {
        named_modules.value = json.result;
    });
}
function refresh() {
    if (cur_tab.value == "access") {
        flush_table();
    } else {
        flush_named_module();
    }
}
onMounted(refresh)
function toggle_chain(table: TableView) {
    table.show_chain = !table.show_chain;
}
function del_table(table: TableView) {
    if (confirm('您确定要删除表吗？')) {
        fetch(get_whm_url('del_table') + '&name=' + table.name).then((res) => res.json()).then((json) => {
            flush_table();
        });
    }
}
function new_table() {
    let table = prompt("输入表的名字");
    if (table == null) {
        return;
    }
    fetch(get_whm_url('add_table') + '&name=' + table).then((res) => res.json()).then((json) => {
        flush_table();
    });
}
const cur_tab = ref("access")
const curEditNamedModule = ref(<EditNamedModule | null>null);
function switch_tab(tab: string) {
    cur_tab.value = tab;
    refresh();
}
function add_named_module(type: number) {
    curEditNamedModule.value = { type: type, edit: false };
}
function del_named_module(type: number, m: NamedModule) {
    if (!confirm("确定要删除命名模块" + m.name + "吗?")) {
        return;
    }
}
function edit_named_module(type: number, m: NamedModule) {
    let param: Record<string, any> = { access: props.access, name: m.name, type: type }
    if (props.vh) {
        param["vh"] = props.vh;
    }
    whm_get('get_named_module', param).then((json) => {
        curEditNamedModule.value = { type: type, m: json.result, name: m.name, edit: true };
    });
}
function cancel_edit_named_module() {
    curEditNamedModule.value = null;
}
function select_module(m:ModuleBase) {
    if (curEditNamedModule.value==null) {
        return;
    }
    curEditNamedModule.value.m = m;
}
</script>
<template>
    <div>
        [<a href=# @click="switch_tab('access')">控制规则</a>]
        [<a href=# @click="switch_tab('named_module')">命名模块</a>]
        [<a href=# @click="refresh()">刷新</a>]
    </div>
    <!-- access list -->
    <template v-if="cur_tab == 'access'">
        <DefaultAction :access=props.access></DefaultAction>

        <div v-if="tables == null">正在加载</div>
        <template v-else>
            <div v-for="table in tables" class="access_table">
                <a href=# @click="toggle_chain(table)">{{ table.name }}</a> 引用:{{ table.refs }}
                [<a href=# @click="del_table(table)">删除</a>]
                <div v-if="table.show_chain">
                    <TableView :access=props.access :name=table.name :vh=props.vh />
                </div>
            </div>
        </template>
        [<a @click="new_table" href=#>新建表</a>]
    </template>
    <!-- named_module -->
    <template v-else>
        <div v-for="(module, index) in [named_modules?.acl, named_modules?.mark]">
            {{ index == 0 ? "匹配模块" : "标记模块" }} [<a href=# @click="add_named_module(index)">增加</a>]
            <table border="1" cellspacing="0">
                <tr>
                    <th>操作</th>
                    <th>名称</th>
                    <th>模块</th>
                    <th>信息</th>
                </tr>
                <tr v-for="m in module">
                    <td><a href=# @click="del_named_module(index, m)">删除</a></td>
                    <td><a href=# @click="edit_named_module(index, m)">{{ m.name }}</a></td>
                    <td>{{ m.module }}</td>
                    <td>{{ m.html }}</td>
                </tr>
            </table>
            <hr>
        </div>
        <div v-if="curEditNamedModule != null" class="edit_chain">
            <form id="form" name="accessaddform" onsubmit="return false;">
                <table border="1" cellspacing="0">
                    <tr>
                        <td>名称</td>
                        <td>
                            <template v-if="!curEditNamedModule.edit">
                                <input name="module" :value="curEditNamedModule.name"/>
                            </template>
                            <template v-else>
                            {{ curEditNamedModule.name }}
                            </template>
                        </td>
                    </tr>
                    <tr>
                        <td>模块</td>
                        <td>
                            <template v-if="curEditNamedModule.m">
                            {{ curEditNamedModule.m.module }}
                            </template>
                            <template v-else>
                                <ModuleSelectView @select_module="select_module($event)" :access="props.access" :type="curEditNamedModule.type"/>
                            </template>
                        </td>
                    </tr>
                    <tr>
                        <td>配置</td>
                        <td>
                            <template v-if="curEditNamedModule.m">
                            <input type="hidden" name="begin_sub_form"
                                :value="(curEditNamedModule.type == 0 ? 'acl_' : 'mark_') + curEditNamedModule.m?.module" />
                            <ModuleView :module="curEditNamedModule.m" />
                            <input type="hidden" name="end_sub_form" value='1' />
                            </template>
                        </td>
                    </tr>
                    <tr>
                        <td colspan="2">
                            <button @click="">确定</button>
                            <button @click="cancel_edit_named_module()">取消</button>
                        </td>
                    </tr>
                </table>
            </form>
        </div>
    </template>
</template>
<style>
.access_table {
    border: 1px solid;
    margin: 2px;
    padding: 2px;
}

.edit_chain {
    border: 1px solid #666;
    background: white;
    position: fixed;
    height: 100%;
    left: 300px;
    top: 0;
    width: 100%;
    padding: 0px;
    margin: 1px;
    z-index: 4300;
}
</style>