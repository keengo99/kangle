<script lang="ts">
export default {
    name:"whm",
}
export function whm_get(call_name: string, param?: Record<string, string>): Promise<any> {
    if (param == null) {
        param = {}
    }
    param["whm_call"] = call_name;
    param['format'] = 'json';
    return fetch('/core.whm?' + new URLSearchParams(param).toString()).then((res) => res.json());
}
export function whm_post(call_name: string, formData: FormData|Record<string, string>): Promise<any> {
    return fetch('/core.whm?' + new URLSearchParams({ "whm_call": call_name, "format": "json" }).toString(),
        {
            method: 'POST',
            body: new URLSearchParams(formData as any),
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded',
            },
        }).then((res) => res.json());
}
</script>