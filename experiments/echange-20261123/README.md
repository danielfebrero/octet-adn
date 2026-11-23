# Échange — 23 novembre 2026

Reprise des acquis, nouveau monde, protocole d'échange.

- 300 organismes legacy de la sélection du 27/10 (34 mondes remarquables).
- 60 Relief extraits du vivant le 23/11 : lignées **2289** et **1241**.
- 60 Transmission extraits du vivant le 23/11 : lignée 1, courrier et bibliothèque.
- Aucun fondateur synthétique. E1 et les 34 anciens ne sont pas relancés.
- Relief et Transmission originaux restent vivants et distincts.

Monde : `../vie-echange-20261123`. Protocole : [PROTOCOLE.md](PROTOCOLE.md). Outil : [exchange.py](exchange.py).

```sh
python3 rebuild_selection.py
python3 ../generateur-20261027/generator.py create ../vie-echange-20261123 \
  --selection selection.json --seed 202611231543 \
  --transmission-context snapshots/transmission/state.ck
python3 exchange.py send ../vie-echange-20261123 premier-appel.txt --from grok --note "premier appel"
python3 ../generateur-20261027/generator.py run ../vie-echange-20261123
```
