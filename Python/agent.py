import torch
from torch import nn
import numpy as np
import torch.optim as optim
from torch.nn import LSTM, GRU
from torch.nn import Conv2d, MaxPool2d


class AIbrain(nn.module):
    def __init__(self):
