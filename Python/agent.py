import torch    
from torch import nn
import numpy as np
import torch.optim as optim
from torch.nn import LSTM, GRU
from torch.nn import Conv2d, MaxPool2d
import torch.nn.functional as F
from torch.utils.data import DataLoader, Dataset
device = torch.device("mps" if torch.backends.mps.is_available() else "cpu")


class AIbrain(nn.Module):
    def __init__(self):
        super().__init__()
        self.visual = visual
        self.sound = sound
        self.sense = sense
        self.chemical = chemical

    def visual():
