# Kesken: jaettu kirjasto linkittämättä

Tämä projekti **ei** vielä sisällä jaettua KiCad-kirjastoa, koska mironet-hw-lib:llä ei ole vielä GitHub-remotea.

Ilman sitä projekti ei täytä toistettavuusvaatimusta: tuore klooni avautuisi
puuttuvilla symboleilla. Korjaa ennen kuin skeemaa piirretään pitkälle:

```bash
cd ~/Development/mironet/hw/mironet-hw-lib
gh repo create mironet-hw-lib --public --source=. --remote=origin --push

cd /Users/miro/Development/mironet/hw/vallox-rs485-controller
git submodule add https://github.com/Miroeilola/mironet-hw-lib lib/mironet-hw-lib
git commit -m "chore: add shared hardware library as a submodule"
rm docs/pending-submodule.md
```
