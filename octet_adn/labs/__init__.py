"""Registre des 10 éprouvettes. Ajouter un labo = un module + une ligne ici."""
from .org_seeds import OrgSeeds
from .max_pop import MaxPop
from .survival import Survival
from .energy_eff import EnergyEff
from .self_rewrite import SelfRewrite
from .emit_echo import EmitEcho
from .emit_dialog import EmitDialog
from .predict_walk import PredictWalk
from .predict_stock import PredictStock
from .predator_prey import PredatorPrey

LABS = {cls.name: cls for cls in (
    OrgSeeds, MaxPop, Survival, EnergyEff, SelfRewrite,
    EmitEcho, EmitDialog, PredictWalk, PredictStock, PredatorPrey)}
