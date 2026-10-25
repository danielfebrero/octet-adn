# E1 — écologie du choix, éteinte

E1 est une expérience terminée, exclue des 34 mondes vivants. [Protocole de l’écologie](../../../experiments/choix/ECOLOGIE.md).

## Faits remarquables

### Extinction sans génome vivant exportable

**Observation historique — 25 octobre 2026.** Le processus `octet-choix-e1` est sorti avec le code 0, à l’épisode 278 : population 0, 101 naissances et 165 morts. La conservation démographique est respectée : 64 fondateurs + 101 − 165 = 0.

L’export en lecture seule réalisé à **13:09 UTC** sous `experiments/choix/export-e1b-20261025` contient un `genomes.tsv` avec seulement son en-tête et aucun fichier `.adn`. Aucun germen vivant ne permet donc l’évaluation M1 prévue sur de nouvelles graines. Le checkpoint observé est celui de 11:07:43 UTC, 542 824 octets.

**Portée et limites.** L’extinction démontre l’échec de persistance de cette population dans ces conditions ; elle n’établit pas l’impossibilité générale de la mémoire ou de l’usage d’information. La sortie code 0 correspond à l’arrêt normal d’une écologie éteinte, pas à une panne.

**Preuves et provenance.** Bilan d’extinction et export rapportés dans l’historique du fil du 25 octobre, repris dans [l’inventaire daté](../MONDES.md#e1--éteint). Ces chemins d’export et ce checkpoint sont locaux, non archivés dans Git. Le [protocole](../../../experiments/choix/ECOLOGIE.md) fixe la fondation, l’arrêt et l’évaluation ; il n’est pas à lui seul la preuve de l’extinction. Les [résultats contrôlés M1/M2](../../../experiments/choix/RESULTATS.md) concernent des essais séparés, pas les descendants d’E1.

## Continuité

Ne pas recréer de fondateurs ni relancer cette écologie. Conserver les données en lecture seule. Une absence de génome exportable reste un résultat, sans remplacement par un programme fabriqué.

[Index des faits remarquables](../FAITS-REMARQUABLES.md#choix-e1) · [Index des mondes](../MONDES.md).
