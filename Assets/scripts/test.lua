local TestScript = {}
TestScript.__index = TestScript

function TestScript:OnCreate()
    print("Lua: TestScript Created on Entity " .. self.id)
    self.timer = 0
end

function TestScript:OnUpdate(dt)
    self.timer = self.timer + dt
    if self.timer > 1.0 then
        print("Lua: Updating Entity " .. self.id .. " dt=" .. dt)
        self.timer = 0
    end
end

return TestScript
