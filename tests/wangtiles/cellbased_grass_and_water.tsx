<?xml version="1.0" encoding="UTF-8"?>
<tileset version="1.10" tiledversion="1.12.2" name="CellBasedGrassAndWater" tilewidth="64" tileheight="64" tilecount="24" columns="4">
 <grid orientation="isometric" width="64" height="32"/>
 <image source="grass_and_water.png" width="256" height="384"/>
 <wangsets>
  <wangset name="Cell Grass and Water" type="corner" tile="6" cellbased="1">
   <wangcolor name="grass" color="#72ff62" tile="0" probability="1"/>
   <wangcolor name="water" color="#52c9ff" tile="23" probability="1"/>
   <wangtile tileid="0" wangid="0,1,0,1,0,1,0,1" center="1"/>
   <wangtile tileid="1" wangid="0,1,0,1,0,1,0,1" center="1"/>
   <wangtile tileid="2" wangid="0,1,0,1,0,1,0,1" center="1"/>
   <wangtile tileid="3" wangid="0,0,0,0,0,0,0,0" center="1"/>
   <wangtile tileid="22" wangid="0,2,0,2,0,2,0,2" center="2"/>
   <wangtile tileid="23" wangid="0,0,0,0,0,0,0,0" center="2"/>
  </wangset>
 </wangsets>
</tileset>
