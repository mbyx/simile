#include <LEDMatrixDriver.hpp>

const uint8_t LEDMATRIX_CS_PIN = 5;
const uint8_t LEDMATRIX_SEGMENTS = 1;
const uint8_t LEDMATRIX_WIDTH = LEDMATRIX_SEGMENTS * 8;

LEDMatrixDriver lmd(LEDMATRIX_SEGMENTS, LEDMATRIX_CS_PIN);

struct Animation {
    String name;
    byte frames[50][8];  // Up to 50 frames per animation
    float durations[50];
    int frameCount;
};

Animation animations[10];  // Up to 10 animations
int numAnimations = 0;
int currentAnimation = -1;  // -1 means play all animations
unsigned long lastFrameTime = 0;
int currentFrame = 0;
int currentAnimationIndex = 0;

void setup() {
    Serial.begin(115200);
    lmd.setEnabled(true);
    lmd.setIntensity(1);
}

void loop() {
    checkSerial();
    playAnimations();
}

void checkSerial() {
    if (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        line.trim();
        
        Serial.println("Received: " + line);
        
        if (line.startsWith("PLAY:")) {
            String animName = line.substring(5);
            Serial.println("Playing animation: " + animName);
            playSpecificAnimation(animName);
        } else if (line.startsWith("ANIM:")) {
            Serial.println("Processing animation data...");
            parseAnimationData(line.substring(5));
        } else if (line == "CLEAR") {
            Serial.println("Clearing animations...");
            numAnimations = 0;
            currentAnimation = -1;
            currentFrame = 0;
            currentAnimationIndex = 0;
            lmd.clear();
            lmd.display();
        } else if (line == "PLAY_ALL") {
            Serial.println("Playing all animations...");
            currentAnimation = -1;
            currentFrame = 0;
            currentAnimationIndex = 0;
            lastFrameTime = millis();
        } else if (line == "DEBUG") {
            printAnimationDebug();
        }
    }
}

void parseAnimationData(String data) {
    // Format: "AnimName|frame1:duration1 frame2:duration2 ..."
    int pipeIndex = data.indexOf('|');
    if (pipeIndex == -1) {
        Serial.println("Error: No pipe found in animation data");
        return;
    }
    
    String animName = data.substring(0, pipeIndex);
    String frameData = data.substring(pipeIndex + 1);
    
    Serial.println("Animation name: " + animName);
    Serial.println("Frame data length: " + String(frameData.length()));
    
    int animIndex = -1;
    for (int i = 0; i < numAnimations; i++) {
        if (animations[i].name == animName) {
            animIndex = i;
            break;
        }
    }
    
    if (animIndex == -1 && numAnimations < 10) {
        animIndex = numAnimations;
        animations[animIndex].name = animName;
        animations[animIndex].frameCount = 0;
        numAnimations++;
        Serial.println("Created new animation at index: " + String(animIndex));
    }
    
    if (animIndex == -1) {
        Serial.println("Error: No space for new animation");
        return;
    }
    
    animations[animIndex].frameCount = 0;
    int idx = 0;
    
    while (idx < frameData.length() && animations[animIndex].frameCount < 50) {
        while (idx < frameData.length() && frameData.charAt(idx) == ' ') {
            idx++;
        }
        
        if (idx + 64 >= frameData.length()) break;
        
        byte frame[8] = {0};
        bool validFrame = true;
        
        for (int i = 0; i < 8; i++) {
            byte b = 0;
            for (int j = 0; j < 8; j++) {
                if (idx + i * 8 + j >= frameData.length()) {
                    validFrame = false;
                    break;
                }
                char c = frameData.charAt(idx + i * 8 + j);
                b <<= 1;
                if (c == '1') b |= 1;
                else if (c != '0') {
                    Serial.println("Invalid character in frame data: " + String(c));
                    validFrame = false;
                    break;
                }
            }
            if (!validFrame) break;
            frame[i] = b;
        }
        
        if (!validFrame) break;
        
        if (idx + 64 >= frameData.length() || frameData.charAt(idx + 64) != ':') {
            Serial.println("Error: Missing colon after frame data");
            break;
        }
        
        int durStart = idx + 65;
        int durEnd = frameData.indexOf(' ', durStart);
        if (durEnd == -1) durEnd = frameData.length();
        
        String durStr = frameData.substring(durStart, durEnd);
        float duration = durStr.toFloat();
        
        if (duration <= 0) {
            Serial.println("Error: Invalid duration: " + durStr);
            break;
        }
        
        int frameIdx = animations[animIndex].frameCount;
        for (int i = 0; i < 8; i++) {
            animations[animIndex].frames[frameIdx][i] = frame[i];
        }
        animations[animIndex].durations[frameIdx] = duration;
        animations[animIndex].frameCount++;
        
        Serial.println("Added frame " + String(frameIdx) + " with duration " + String(duration));
        
        idx = durEnd + 1;
    }
    
    Serial.println("Total frames parsed: " + String(animations[animIndex].frameCount));
}

void playSpecificAnimation(String animName) {
    for (int i = 0; i < numAnimations; i++) {
        if (animations[i].name == animName) {
            currentAnimation = i;
            currentFrame = 0;
            lastFrameTime = millis();
            return;
        }
    }
}

void playAnimations() {
    if (numAnimations == 0) return;
    
    unsigned long currentTime = millis();
    
    if (currentAnimation == -1) {
        if (currentAnimationIndex >= numAnimations) {
            currentAnimationIndex = 0;
        }
        
        Animation& anim = animations[currentAnimationIndex];
        if (anim.frameCount == 0) {
            currentAnimationIndex++;
            return;
        }
        
        if (currentTime - lastFrameTime >= anim.durations[currentFrame] * 1000) {
            currentFrame++;
            if (currentFrame >= anim.frameCount) {
                currentFrame = 0;
                currentAnimationIndex++;
            }
            lastFrameTime = currentTime;
        }
        
        if (currentAnimationIndex < numAnimations) {
            displayFrame(animations[currentAnimationIndex].frames[currentFrame]);
        }
    } else {
        Animation& anim = animations[currentAnimation];
        if (anim.frameCount == 0) return;
        
        if (currentTime - lastFrameTime >= anim.durations[currentFrame] * 1000) {
            currentFrame++;
            if (currentFrame >= anim.frameCount) {
                currentFrame = 0;
            }
            lastFrameTime = currentTime;
        }
        
        displayFrame(anim.frames[currentFrame]);
    }
}

void displayFrame(byte* frame) {
    lmd.clear();
    drawSprite(frame, 0, 0, 8, 8);
    lmd.display();
}

void drawSprite(byte* sprite, int x, int y, int width, int height) {
    for (int iy = 0; iy < height; iy++) {
        byte row = sprite[iy];
        for (int ix = 0; ix < width; ix++) {
            bool pixel = row & (0x80 >> ix);
            // Rotate 90 degrees clockwise: (x,y) -> (height-1-y, x)
            int rotatedX = height - 1 - iy;
            int rotatedY = ix;
            lmd.setPixel(x + rotatedX, y + rotatedY, pixel);
        }
    }
}

void printAnimationDebug() {
    Serial.println("=== Animation Debug ===");
    Serial.println("Total animations: " + String(numAnimations));
    for (int i = 0; i < numAnimations; i++) {
        Serial.println("Animation " + String(i) + ": " + animations[i].name + 
                      " (frames: " + String(animations[i].frameCount) + ")");
        for (int f = 0; f < animations[i].frameCount; f++) {
            Serial.println("  Frame " + String(f) + ": duration=" + String(animations[i].durations[f]));
        }
    }
    Serial.println("Current state: anim=" + String(currentAnimation) + 
                  " frame=" + String(currentFrame) + 
                  " animIdx=" + String(currentAnimationIndex));
    Serial.println("=====================");
}
