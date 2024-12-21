<script lang="ts" setup>
import FooterView from './FooterView.vue';
</script>
<template>
    <div>
    [<RouterLink to='sserver'>单节点服务器 </RouterLink>]
    [<RouterLink to='mserver'>多节点服务器</RouterLink>]
    [<RouterLink to='api'>api扩展</RouterLink>]
    [<RouterLink to='cmd'>命令</RouterLink>]
    [<RouterLink to='dso'>dso</RouterLink>]
    </div>
    <div>
    <RouterView />
    </div>
    <FooterView/>
</template>