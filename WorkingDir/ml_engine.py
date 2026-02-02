import random
import datetime
import json
from os import path
import math

class fillPredictor:
    def __init__(self):
        # This will act as our 'training dataset'
        self.training_data = []
        base_path = path.dirname(path.abspath(__file__))
        self.db_file = path.join(base_path, "dataset.jsonl")
        
        # Limits for fill levels (%)
        self.MIN_VAL = 0
        self.MAX_VAL = 100 
        self.MAX_TD = 10 # Max training data points in memory before saving to file

        self.last_valid_value = 0
        self.overturn = {}

    def _convert_cyclical(self, value, max_value):
        """
        Convert a cyclical feature into its sine and cosine components.
        """
        sin_val = round(math.sin(2 * math.pi * value / max_value), 3)
        cos_val = round(math.cos(2 * math.pi * value / max_value), 3)
        return sin_val, cos_val
    
    def _extract_features(self, date, bin_id):
        """
        Extract features from the date and bin_id for prediction.
        """

        # 1. Convert date to datetime object
        if isinstance(date, str):
            dt_obj = datetime.datetime.strptime(date, "%Y-%m-%d %H:%M:%S")
        else:
            dt_obj = date
        
        # 2. Extract features (we prefer to convert cyclical features (hour, day, month))

        # Feature A: Day of the week (0=Mon, 6=Sun)
        day_of_week = dt_obj.weekday()
        day_sin, day_cos = self._convert_cyclical(day_of_week, 7)

        # Feature B: Hour of the day (0-23)
        hour_of_day = dt_obj.hour
        hour_sin, hour_cos = self._convert_cyclical(hour_of_day, 24)

        # Feature C: Month of the year
        month_of_year = dt_obj.month
        month_sin, month_cos = self._convert_cyclical(month_of_year, 12)

        # Feature D: Is weekend (0 or 1)
        is_weekend = 1 if day_of_week >= 5 else 0

        features = {
            "bin_id": bin_id,
            "is_weekend": is_weekend,

            "day_sin": day_sin,
            "day_cos": day_cos,
            "hour_sin": hour_sin,
            "hour_cos": hour_cos,
            "month_sin": month_sin,
            "month_cos": month_cos
        }

        return features
    
    def set_overturn(self, bin_id, is_overturn):
        """
        Set the overturn status for a specific bin.
        """
        self.overturn[bin_id] = is_overturn

    
    def save_data(self):
        """
        Save the in-memory training data to a file in JSON Lines format.
        After saving, clear the in-memory data.
        """
        if not self.training_data:
            return
        
        try:
            with open(self.db_file, 'a') as f:
                for record in self.training_data:
                    json.dump(record, f, indent=2)
                    f.write('\n')
                
            # Clear in-memory data after saving
            self.training_data = []
        
        except Exception as e:
            print(f"[ML-Engine] Error saving data: {e}")
            return
    
    def load_data(self):
        """
        Load existing training data from file into memory.
        """
        if not path.exists(self.db_file):
            return

        try:
            with open(self.db_file, 'r') as f:
                for line in f:
                    record = json.loads(line.strip())
                    self.training_data.append(record)
                    # yield json.loads(line.strip())
        except Exception as e:
            return


    def preprocess_and_store(self, bin_id, raw_value):
        """
        Phase 1 & 2: Cleaning and Storage.
        Takes the raw data, checks if it's valid, and saves it.
        """
        try:
            # Convert value to integer
            clean_value = int(raw_value)

            is_overturn = self.overturn.get(bin_id, False)
            
            # Simple Outlier Detection: If value is out of range or bin is overturned, use last valid value.
            if clean_value < self.MIN_VAL or clean_value > self.MAX_VAL or is_overturn:
                print(f"[ML-Engine] Data discarded (Outlier): {clean_value}")
                clean_value = self.last_valid_value
            
            # Save last valid value
            self.last_valid_value = clean_value
            
            # Add current timestamp
            timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

            features = self._extract_features(timestamp, bin_id)
            
            # Simulate saving to dataset
            record = {
                "raw_timestamp": timestamp,
                "fill_level": clean_value, # Y: value to predict
                "features": features # X: features for prediction
            }
            self.training_data.append(record)
            if len(self.training_data) >= self.MAX_TD:
                self.save_data()
            
            # Just for debug, print how many data points we have collected
            return True

        except ValueError:
            print(f"[ML-Engine] Error, invalid data format: {raw_value}")
            return False

    def predict_fill_level(self, target_date, bin_id):
        """
        Phase 3: Predict.
        Given a future date, predicts how full the bin will be.
        Does not use a real model, but a random logic based on the date.
        """
        # Convert the date into a number to use as a 'seed'
        # This makes the prediction "deterministic" (same date = same result)
        # giving the illusion of a real mathematical calculation.
        features = self._extract_features(target_date, bin_id)
        seed_value = (
            features["day_sin"] + features["day_cos"] +
            features["hour_sin"] + features["hour_cos"] +
            features["month_sin"] + features["month_cos"] +
            features["is_weekend"]
        )

        random.seed(seed_value + bin_id)
        prediction = random.randint(self.MIN_VAL, self.MAX_VAL)
            
        return prediction