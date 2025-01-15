<script lang="ts" setup>
import { onMounted, ref } from 'vue';
import ModuleView, { type ModuleBase } from './ModuleView.vue';
import ModuleSelectView from './ModuleSelectView.vue'
import NamedModuleSelectView ,{type NamedModuleSelect} from './NamedModuleSelectView.vue';

export interface Module extends ModuleBase {
    is_or: number,
    revers: number,
    ref?: string
}
export interface ChainKey {
    file: string,
    index: number,
    id: number,
}

export interface ChainValue {
    hit: number,
    acl?: Module[],
    mark?: Module[],
}
export interface Chain {
    add?: boolean,
    action: string,
    jump?: string,
    k: ChainKey,
    v: ChainValue,
}

const props = defineProps<{ chain: Chain, access: string, table: string, vh?: string }>()
//console.log("file=" + probs.chain.file);
const chain_value = ref(<ChainValue | null>null);
const emit = defineEmits<{ cancel_chain: [], submit_chain: [] }>();
function submit_chain() {
    const form: any = document.getElementById("form");
    const formData = new FormData(form);
    if (props.vh) {
        formData.append("vh", props.vh);
    }
    formData.append("access", props.access);
    formData.append("table", props.table);
    formData.append("file", props.chain.k.file);
    formData.append("index", props.chain.k.index.toString());
    formData.append("id", props.chain.k.id.toString());
    if (props.chain.add) {
        formData.append("add", "1");
    }
    fetch('/core.whm?whm_call=edit_chain&format=json', {
        method: 'POST',
        body: new URLSearchParams(formData as any),
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
        },
    }).then((res) => res.json()).then((json) => {
        emit('submit_chain');
    });
}
function cancel_chain() {
    emit("cancel_chain");
}
function updateModule(type: number, index: number, module: string) {
    if (chain_value.value == null) {
        return;
    }
    if (type == 0) {
        if (chain_value.value?.acl == null) {
            return;
        }
        chain_value.value.acl[index].module = module
    } else {
        if (chain_value.value?.mark == null) {
            return
        }
        chain_value.value.mark[index].module = module
    }
}
function UpModule(type: number, index: number) {
    if (index == 0 || chain_value.value == null) {
        return;
    }
    
    if (type == 0) {
        if (chain_value.value?.acl == null) {
            return;
        }
        var m = chain_value.value.acl[index];
        chain_value.value.acl[index] = chain_value.value.acl[index - 1];
        chain_value.value.acl[index - 1] = m;
    } else {
        if (chain_value.value?.mark == null) {
            return
        }
        var m = chain_value.value.mark[index];
        chain_value.value.mark[index] = chain_value.value.mark[index - 1];
        chain_value.value.mark[index - 1] = m;
    }
}
function delModule(type: number, index: number) {
    if (chain_value.value == null) {
        return;
    }
    if (type == 0) {
        if (chain_value.value?.acl == null) {
            return;
        }
        chain_value.value.acl.splice(index, 1)
    } else {
        if (chain_value.value?.mark == null) {
            return
        }
        chain_value.value.mark.splice(index, 1)
    }
}
/*
function addModule(type: number, index: number) {
    if (chain_value.value == null) {
        return;
    }
    let emptyModule: Module = {
        module:"",
        html: "",
        is_or: 0,
        revers: 0,
    }
    if (type == 0) {
        if (chain_value.value?.acl == null) {
            chain_value.value.acl = <Module[]>[];
        }
        if (index == -1) {
            chain_value.value.acl.push(emptyModule);
            return;
        }
        chain_value.value.acl.splice(index, 0, emptyModule)
    } else {
        if (chain_value.value?.mark == null) {
            chain_value.value.mark = <Module[]>[];
        }
        if (index == -1) {
            chain_value.value.mark.push(emptyModule);
            return;
        }
        chain_value.value.mark.splice(index, 0, emptyModule)
    }
}
*/
function addNamedModule(type: number, mb: NamedModuleSelect) {
    if (chain_value.value == null) {
        return;
    }
    let m: Module = {
        is_or: 0,
        revers: 0,
        html: "",
        ref: mb.name,
        module: mb.module
    };
    if (type == 0) {
        if (chain_value.value?.acl == null) {
            chain_value.value.acl = <Module[]>[];
        }
        chain_value.value.acl.push(m);
    } else {
        if (chain_value.value?.mark == null) {
            chain_value.value.mark = <Module[]>[];
        }
        chain_value.value.mark.push(m);
    }
}
function addModule(type: number, mb: ModuleBase) {
    if (chain_value.value == null) {
        return;
    }
    let m: Module = {
        is_or: 0,
        revers: 0,
        html: mb.html,
        module: mb.module
    };
    if (type == 0) {
        if (chain_value.value?.acl == null) {
            chain_value.value.acl = <Module[]>[];
        }
        chain_value.value.acl.push(m);
    } else {
        if (chain_value.value?.mark == null) {
            chain_value.value.mark = <Module[]>[];
        }
        chain_value.value.mark.push(m);
    }
}
onMounted(() => {
    if (!props.chain.add) {
        fetch('/core.whm?whm_call=get_chain&access=' + props.access +
            '&table=' + props.table +
            '&file=' + props.chain.k.file +
            '&index=' + props.chain.k.index +
            (props.vh ? '&vh=' + props.vh : "") +
            '&id=' + props.chain.k.id +
            '&format=json').then((res) => res.json()).then((json) => {
                chain_value.value = json.result;
            });
    } else {
        chain_value.value = { hit: 0 }
    }
}
);
</script>
<template>
    <form id="form" name="accessaddform" onsubmit="return false;" v-if="chain_value">
        <table border="1" cellspacing="0">
            <tr>
                <td>目标</td>
                <td>
                    {{ props.chain.k.file }} {{ props.chain.k.id }}
                </td>
            </tr>
            <template v-for="(modules, type) in [chain_value.acl, chain_value.mark]">
                <tr v-for="(m, index) in modules">
                    <td>
                        [<a href=# @click="delModule(type, index)">删</a>]
                        [<a href=# @click="UpModule(type, index)">UP</a>]
                        <span v-if="m.module">
                            {{ m.module }}
                        </span>
                    </td>
                    <td>

                        <input type="hidden" name="begin_sub_form" :value="(type == 0 ? 'acl_' : 'mark_') + m.module" />
                        <template v-if="m.ref">
                            <input type="hidden" name="ref" :value="m.ref" />命名模块:{{ m.ref }}
                        </template>
                        <template v-else>
                            <ModuleView :module="m" />
                        </template>
                        <input type="hidden" name="end_sub_form" value='1' />

                    </td>
                </tr>
                <tr>
                    <td colspan="2">
                        <ModuleSelectView @select_module="addModule(type, $event)" :access="props.access" :type="type" />
                        <NamedModuleSelectView @select_module="addNamedModule(type, $event)" :access="props.access"
                            :type="type" />
                    </td>
                </tr>
            </template>
            <tr>
                <td colspan="2">
                    <button @click="submit_chain()">确定</button>
                    <button @click="cancel_chain()">取消</button>
                </td>
            </tr>
        </table>
    </form>
</template>