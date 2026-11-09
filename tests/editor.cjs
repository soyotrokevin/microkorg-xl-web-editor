const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {chromium} = require('playwright');
const root = path.resolve(__dirname, '..');
(async () => {
  const browser = await chromium.launch(process.env.CHROME_PATH ? {executablePath:process.env.CHROME_PATH} : {});
  try {
    const page = await browser.newPage({viewport:{width:1280,height:720}});
    const errors=[];page.on('pageerror',e=>errors.push(e.message));
    await page.route('https://editor.test/**', async route => {
      const pathname=new URL(route.request().url()).pathname;
      const file=path.join(root, pathname==='/'?'mkxl_editor.html':pathname.slice(1));
      if(!file.startsWith(root+path.sep)||!fs.existsSync(file))return route.fulfill({status:404});
      return route.fulfill({body:fs.readFileSync(file),contentType:file.endsWith('.html')?'text/html':file.endsWith('.json')?'application/json':'application/octet-stream'});
    });
    await page.goto('https://editor.test/');
    const names=new Set();
    for(const tab of ['Synthesizer','Voice','FX','Matrix','Vocoder']){
      await page.getByRole('tab',{name:tab,exact:true}).click();
      for(const name of await page.locator('[data-control]').evaluateAll(nodes=>nodes.map(n=>n.dataset.control))){assert(!names.has(name),'Duplicate visible control: '+name);names.add(name);}
      const layout=await page.evaluate(()=>{const v=document.getElementById('view');return {scroll:v.scrollHeight>v.clientHeight,horizontal:document.documentElement.scrollWidth>innerWidth};});
      console.log(tab,layout);assert(!layout.horizontal);
      if(process.env.CAPTURE_DIR&&['Synthesizer','Voice','FX'].includes(tab))await page.screenshot({path:path.join(process.env.CAPTURE_DIR,tab.toLowerCase()+'.png')});
      if(tab==='Synthesizer')assert(!layout.scroll,'Main dashboard should fit at 1280 × 720');
    }
    assert.equal(names.size,130,'Every catalog control must be accessible');
    await page.evaluate(()=>{window.sent=[];midiOut={send:bytes=>window.sent.push([...bytes])};});
    await page.getByRole('tab',{name:'FX',exact:true}).click();
    const fx=page.locator('[data-control="FX1_TYPE"] select');assert.equal(await fx.locator('option').count(),18);await fx.selectOption('11');
    assert.deepEqual(await page.evaluate(()=>sent.pop()),[240,66,48,126,65,80,0,1,0,11,0,247]);
    await page.getByRole('tab',{name:'Synthesizer',exact:true}).click();
    await page.locator('[data-control="SYX_ATTACK_1"]').focus();await page.keyboard.press('End');
    assert.deepEqual(await page.evaluate(()=>sent.pop()),[240,66,48,126,65,17,0,0,1,127,0,247]);
    await page.evaluate(()=>{sendCtl(DATA.controls.find(c=>c.name==='SYX_KEY_SYNC'),20);sendCtl(DATA.controls.find(c=>c.name==='SYX_KEY_SYNC_164'),90);});
    assert.deepEqual(await page.evaluate(()=>[vals.SYX_KEY_SYNC,vals.SYX_KEY_SYNC_164]),[20,90]);
    await page.evaluate(()=>{const block=new Uint8Array(346);block[286]=139;block[310]=145;applyDump([240,66,48,126,64,...pack7(block),247]);});
    await page.getByRole('tab',{name:'FX',exact:true}).click();assert.equal(await fx.inputValue(),'11');assert.equal(await page.locator('[data-control="FX2_TYPE"] select').inputValue(),'17');
    await page.getByRole('tab',{name:'Presets',exact:true}).click();await page.waitForFunction(()=>document.querySelectorAll('#plist .send').length===128);
    await page.evaluate(async()=>{const bytes=new Uint8Array(528);bytes.set(new TextEncoder().encode('6140PrgD'));bytes.set(new TextEncoder().encode('LocalOne'),32);await loadBank(new File([bytes],'local.mkxl_all'));});
    await page.getByRole('tab',{name:'FX',exact:true}).click();await page.getByRole('tab',{name:'Presets',exact:true}).click();await page.getByText('LocalOne',{exact:true}).waitFor();
    await page.setViewportSize({width:390,height:844});await page.getByRole('tab',{name:'Synthesizer',exact:true}).click();assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
    assert.deepEqual(errors,[]);console.log('PASS: 130 controls, SysEx framing, FX masks, independent state, automatic bank loading and responsive layout');
  }finally{await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
