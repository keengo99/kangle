<script lang="ts" setup>
import Footer from "./FooterView.vue"
</script>
<template>
    <div>
    [<RouterLink to='listen'>服务</RouterLink>]
    [<RouterLink to='cache'>缓存</RouterLink>]
    [<RouterLink to='log'>日志</RouterLink>]
    [<RouterLink to='resource'>资源限制</RouterLink>]
    [<RouterLink to='io'>数据交换</RouterLink>]
    [<RouterLink to='other'>其它选项</RouterLink>]
    [<RouterLink to='manage'>管理选项</RouterLink>]
    </div>
    <div>
    <RouterView />
    </div>
    <Footer/>
</template>