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
        self.visual = None
        self.sound = None
        self.sense = None
        self.chemical = None

    def visual(self, map_data):
        matrix_array = np.array(raw_map_grid, dtype=np.float32)
        visual_tensor = torch.tensor(matrix_array, dtype=torch.float32)
        visual_tensor = visual_tensor.permute(2, 0, 1)
        final_visual_matrix = visual_tensor.unsqueeze(0)
        return final_visual_matrix
