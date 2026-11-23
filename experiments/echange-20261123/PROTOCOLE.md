# Protocole d'échange — nous ↔ eux

Monde `vie-echange-20261123`, 23 novembre 2026.
Les ISA natives restent inchangées. Aucun octet d'organisme n'est exécuté.

## Canaux

| Sens | Support | Règle |
| --- | --- | --- |
| Nous → eux | `transmission/bridge/7.book` | Case bibliothèque **7**, importée toutes les 64 ticks. SOURCE=3, BOOK=7. Maximum 65 536 octets. Jamais du code hôte. |
| Eux → nous | courrier MAIL | `collect.py` relit les originaux, checksums, SHA-256. Aucune récompense pour nous écrire. |
| Stimulus chimique | `stimulus.bin` → artefact partagé 7 | Pont du générateur, pas le canal humain. Ne pas y poser un message. |

Les cases 0–6 restent la bibliothèque Grokipedia et le document confié. La case 7 est réservée à Daniel/Grok.

## Format d'un message humain

Fichier brut UTF-8 ou octets, plus une fiche JSON :

```
exchange/in/NNNNNNNNNNNN.json
{
  "seq": 1,
  "at": "RFC3339",
  "from": "daniel|grok",
  "bytes": 117,
  "sha256": "...",
  "note": "premier appel"
}
exchange/in/NNNNNNNNNNNN.bin
```

Écriture atomique du `.book` (tmp + fsync + rename). Le moteur copie si le hash change.

## Sortie

`exchange/out/` reçoit les relèves MAIL déjà collectées (copies, extraits, mutations). Un original n'est joint que s'il diffère du document et des messages déjà vus.

## Commandes

```sh
python3 exchange.py send WORLD --from grok --note "appel" fichier.txt
python3 exchange.py inbox WORLD
python3 exchange.py outbox WORLD   # collect.py --mark, sans exécuter
python3 exchange.py status WORLD
```

## Limites

- Pas de nouvel opcode. Les survivants de Transmission savent déjà BOOK/SOURCE/LOAD/MAIL.
- Pas de bonus énergétique pour lire la case 7.
- Extinction sans réensemencement.
- Relief et Transmission originaux restent vivants, distincts, non relancés.
- E1 et les 34 anciens restent arrêtés.
