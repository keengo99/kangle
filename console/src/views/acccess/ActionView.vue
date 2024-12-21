<script lang="ts" setup>
const probs = defineProps<{
    access:string,
    vh?:string,
    table?:string,
}>()
</script>
<template>
    {{ probs.access }} action
</template>