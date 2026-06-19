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
        self.visual_input = None
        self.sound_input = None
        self.sense_input = None
        self.chemical = None

    def visual(self, map_data):
        matrix_array = np.array(map_data, dtype=np.float32)
        visual_tensor = torch.tensor(matrix_array, dtype=torch.float32)
        visual_tensor = visual_tensor.permute(2, 0, 1)
        final_visual_matrix = visual_tensor.unsqueeze(0)
        return final_visual_matrix

    def sound(self, audio_frequencies):
        raw_list = list(audio_frequencies)
        numpy_vector = np.array(raw_list, dtype=np.float32)
        sound_tensor = torch.tensor(numpy_vector, dtype=torch.float32)
        final_sound_vector = sound_tensor.unsqueeze(0)
        return final_sound_vector

    def sense(self, tactile_data):
        numpy_vector = np.array(tactile_data, dtype=np.float32)
        sense_tensor = torch.tensor(numpy_vector, dtype=torch.float32)
        final_sense_vector = sense_tensor.unsqueeze(0)
        return final_sense_vector

    def chemical(self, chemical_data):
        numpy_vector = np.array(chemical_data, dtype=np.float32)
        chemical_tensor = torch.tensor(numpy_vector, dtype=np.float32)
        final_chemical_vector = chemical_tensor.unsqueeze(0)
        return final_chemical_vector
