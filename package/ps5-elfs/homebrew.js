async function main() {
    const PAYLOAD = window.workingDir + '/eboot.elf';

    return {
        mainText: 'ps5-elfs',
        secondaryText: 'Payload host',
        onclick: async () => {
            return {
                path: PAYLOAD,
                daemon: true
            };
        }
    };
}
