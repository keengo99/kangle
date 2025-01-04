<script lang="ts">
export interface ServerNode {  
    host: string,
    port: number,
    proto: string,
    life_time:number,
    self_ip:string,
    idle_size:number,
    total_connect:number,
    total_hit:number,
    total_err:number,
}

</script>