/* Seeds brutes : les fichiers seeds/NOM.adn sont copiés tels quels dans .rodata.
 * Aucun encodage intermédiaire : les octets du génome sont les octets du binaire. */
        .section .rodata
.macro SEED sym, file
        .global \sym, \sym\()_end
        .balign 16
\sym:
        .incbin "\file"
\sym\()_end:
.endm
        SEED adn_forager,  "seeds/forager.adn"
        SEED adn_neural,   "seeds/neural.adn"
        SEED adn_echo,     "seeds/echo.adn"
        SEED adn_dialog,   "seeds/dialog.adn"
        SEED adn_stock,    "seeds/stock.adn"
        SEED adn_social,   "seeds/social.adn"
        SEED adn_predator, "seeds/predator.adn"
        SEED adn_walk,     "seeds/walk.adn"
        SEED adn_fold,     "seeds/fold.adn"
        SEED adn_antibiotic, "seeds/antibiotic.adn"
        SEED adn_commons,  "seeds/commons.adn"
        SEED adn_pandemic, "seeds/pandemic.adn"
        .section .note.GNU-stack,"",@progbits
