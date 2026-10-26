/* SPDX-License-Identifier: GPL-3.0-only
 * Strict, bounded MathQuill LaTeX reader. No JavaScript evaluation or unrestricted
 * Nerdamer command input: only the supported mathematical grammar reaches CAS.
 */
(function(root) {
'use strict';
class InputError extends Error {}
const functions = new Set(['sin','cos','tan','sec','csc','cot','arcsin','arccos','arctan','asin','acos','atan','ln','log','abs','sqrt','exp']);
class Reader {
  constructor(latex, mode) {
    if (typeof latex !== 'string' || latex.length > 700) throw new InputError('Keep the expression under 700 characters.');
    this.s = latex.replace(/\\left\|/g,'abs(').replace(/\\right\|/g,')').replace(/\\(?:left|right)/g,'').replace(/\\[,!; ]/g,' ').replace(/−/g,'-');
    this.i=0; this.depth=0; this.mode=mode;
  }
  skip(){while(/\s/.test(this.s[this.i]||'') && this.i<this.s.length)this.i++;}
  peek(){this.skip();return this.s[this.i]||'';}
  eat(c){if(this.peek()===c){this.i++;return true;}return false;}
  node(type, fields={}){return Object.assign({type},fields);}
  group(open='{', close='}') {
    if (!this.eat(open)) throw new InputError('Complete the fraction, root, or exponent.');
    if(++this.depth>24) throw new InputError('This expression is nested too deeply.');
    const n=this.add();if(!this.eat(close))throw new InputError('Close the brackets and fill each empty space.');this.depth--;return n;
  }
  add(){let a=this.mul();while(this.peek()==='+'||this.peek()==='-'){let op=this.peek();this.i++;a=this.node(op,{a,b:this.mul()});}return a;}
  mul(){let a=this.unary();for(;;){let c=this.peek();if(c==='*'||c==='/'){this.i++;a=this.node(c,{a,b:this.unary()});}else if(c && /[\d.a-zA-Z\\({|]/.test(c)){a=this.node('*',{a,b:this.unary()});}else return a;}}
  unary(){if(this.eat('+'))return this.unary();if(this.eat('-'))return this.node('neg',{a:this.unary()});return this.power();}
  power(){let a=this.atom();if(this.eat('^'))a=this.node('^',{a,b:this.peek()==='{'?this.group():this.unary()});if(this.eat('!'))a=this.node('factorial',{a});return a;}
  atom(){
    let c=this.peek();
    if(c==='(')return this.group('(',')');if(c==='{')return this.group();
    if(c==='|'){this.i++;const n=this.add();if(!this.eat('|'))throw new InputError('Close the absolute-value bars.');return this.node('fn',{name:'abs',a:n});}
    const number=this.s.slice(this.i).match(/^(?:\d+(?:\.\d*)?|\.\d+)/);
    if(number){this.i+=number[0].length;if(!Number.isFinite(Number(number[0])) || Math.abs(Number(number[0]))>1e100)throw new InputError('This number is too large.');return this.node('number',{value:number[0]});}
    let name='';
    if(c==='\\'){this.i++;const m=this.s.slice(this.i).match(/^[a-zA-Z]+/);if(!m)throw new InputError('Unsupported symbol.');name=m[0];this.i+=name.length;
      if(name==='operatorname'){if(!this.eat('{'))throw new InputError('Incomplete function.');const start=this.i;while(this.i<this.s.length&&this.s[this.i]!=='}')this.i++;name=this.s.slice(start,this.i);if(!this.eat('}'))throw new InputError('Incomplete function.');}
      if(name==='cdot'||name==='times'||name==='div')throw new InputError('Enter a value before the operator.');
      if(name==='frac'||name==='dfrac')return this.node('/',{a:this.group(),b:this.group()});
      if(name==='sqrt'){let index=null;if(this.peek()==='[')index=this.group('[',']');const a=this.group();return index?this.node('root',{a,b:index}):this.node('fn',{name:'sqrt',a});}
    } else if(/[a-zA-Z]/.test(c)&&c){const m=this.s.slice(this.i).match(/^[a-zA-Z]+/)[0];if(functions.has(m)||m==='pi'){name=m;this.i+=m.length;}else{name=c;this.i++;}}
    if(name==='x')return this.node('x');if(name==='pi'||name==='e')return this.node('constant',{value:name});
    if(functions.has(name)){
      let base=null, exponent=null;
      if(name==='log'&&this.eat('_'))base=this.peek()==='{'?this.group():this.atom();
      if(this.eat('^'))exponent=this.peek()==='{'?this.group():this.unary();
      if(this.peek()!=='(')throw new InputError('Put the argument of '+name+' in parentheses.');
      let a=this.group('(',')');let n=this.node('fn',{name,a,base});return exponent?this.node('^',{a:n,b:exponent}):n;
    }
    if(!c)throw new InputError('Finish the expression to calculate.');
    throw new InputError('Unsupported symbol or variable. This calculator supports x.');
  }
  read(){
    // Functions may be entered with or without a definition prefix.
    this.s=this.s.replace(/^\s*(?:f\s*\(\s*x\s*\)|y)\s*=\s*/,'');
    this.s=this.s.replace(/\\(?:cdot|times)(?![a-zA-Z])/g,'*').replace(/\\div(?![a-zA-Z])/g,'/');
    const n=this.add();if(this.peek())throw new InputError('Check brackets and remove unsupported notation.');return n;
  }
}
function containsX(n){return n.type==='x'||!!(n.a&&containsX(n.a))||!!(n.b&&containsX(n.b))||!!(n.base&&containsX(n.base));}
function numeric(n,x,mode){
  let a=n.a?numeric(n.a,x,mode):0,b=n.b?numeric(n.b,x,mode):0,v;
  switch(n.type){
  case 'number':v=Number(n.value);break;case 'constant':v=n.value==='pi'?Math.PI:Math.E;break;case 'x':if(x===undefined)throw new InputError('Enter a value for x.');v=x;break;
  case '+':v=a+b;break;case '-':v=a-b;break;case '*':v=a*b;break;case '/':if(b===0)throw new InputError('Division by zero is undefined.');v=a/b;break;
  case 'neg':v=-a;break;case '^':if(a===0&&b===0)throw new InputError('0⁰ is undefined here.');if(Math.abs(b)>10000)throw new InputError('The exponent is too large.');v=Math.pow(a,b);break;
  case 'root':if(b===0)throw new InputError('A root index cannot be zero.');v=a<0&&Number.isInteger(b)&&Math.abs(b%2)===1?-Math.pow(-a,1/b):Math.pow(a,1/b);break;
  case 'factorial':if(!Number.isInteger(a)||a<0||a>170)throw new InputError('Factorial needs an integer from 0 to 170.');v=1;for(let i=2;i<=a;i++)v*=i;break;
  case 'fn':{
    const t=mode==='degrees'?a*Math.PI/180:a, back=mode==='degrees'?180/Math.PI:1;
    switch(n.name){
    case 'sqrt':v=Math.sqrt(a);break;case 'abs':v=Math.abs(a);break;case 'exp':v=Math.exp(a);break;
    case 'ln':if(a<=0)throw new InputError('A logarithm needs a positive argument.');v=Math.log(a);break;
    case 'log':{const base=n.base?numeric(n.base,x,mode):10;if(a<=0||base<=0||base===1)throw new InputError('Use a positive argument and a positive base other than 1.');v=Math.log(a)/Math.log(base);break;}
    case 'sin':v=Math.sin(t);break;case 'cos':v=Math.cos(t);break;
    case 'tan':case 'sec':if(Math.abs(Math.cos(t))<1e-14)throw new InputError('This trigonometric value is undefined.');v=n.name==='tan'?Math.tan(t):1/Math.cos(t);break;
    case 'csc':case 'cot':if(Math.abs(Math.sin(t))<1e-14)throw new InputError('This trigonometric value is undefined.');v=n.name==='csc'?1/Math.sin(t):Math.cos(t)/Math.sin(t);break;
    case 'asin':case 'arcsin':v=Math.asin(a)*back;break;case 'acos':case 'arccos':v=Math.acos(a)*back;break;case 'atan':case 'arctan':v=Math.atan(a)*back;break;
    default:throw new InputError('Unsupported function.');
    }break;}
  default:throw new InputError('Unsupported expression.');
  }
  if(!Number.isFinite(v))throw new InputError('No finite real result. Check the domain and size of your values.');return v;
}
function cas(n,x,mode){
  const a=n.a?cas(n.a,x,mode):'',b=n.b?cas(n.b,x,mode):'';
  switch(n.type){
  case 'number':return n.value;case 'constant':return n.value;case 'x':return '('+x+')';case 'neg':return '(-('+a+'))';
  case '+':case '-':case '*':case '/':case '^':return '(('+a+')'+n.type+'('+b+'))';
  case 'factorial':return 'factorial('+a+')';case 'root':return 'nthroot('+a+','+b+')';
  case 'fn':{
    let name=n.name.replace(/^arc/,'a');if(name==='ln')name='log';
    if(n.name==='log')return 'log('+a+','+(n.base?cas(n.base,x,mode):'10')+')';
    if(['sin','cos','tan','sec','csc','cot'].includes(name))return name+'('+(mode==='degrees'?'('+a+')*pi/180':a)+')';
    if(['asin','acos','atan'].includes(name))return '('+name+'('+a+')'+(mode==='degrees'?'*180/pi':'')+')';
    return name+'('+a+')';
  }
  }
}
function exactSafe(n){
  if(n.type==='^' && (n.b.type!=='number'||Math.abs(Number(n.b.value))>64))return false;
  if(n.type==='factorial' && (n.a.type!=='number'||Number(n.a.value)>50))return false;
  return (!n.a||exactSafe(n.a))&&(!n.b||exactSafe(n.b))&&(!n.base||exactSafe(n.base));
}
function parse(latex){return new Reader(latex).read();}
function calculate(latex,xLatex,mode='degrees',symbolic=root.nerdamer){
  const n=parse(latex);let x,xc;
  if(containsX(n)){const xn=parse(xLatex||'');if(containsX(xn))throw new InputError('The value of x must be a number.');x=numeric(xn,undefined,mode);xc=cas(xn,null,mode);}
  const value=numeric(n,x,mode), decimal=Number(value.toPrecision(12)).toString();let exact='';
  // Large symbolic integer powers/factorials can be expensive. Numeric evaluation
  // remains available; exact calculation is deliberately bounded for live input.
  if(symbolic && Math.abs(value)<1e30 && latex.length<300 && exactSafe(n)){
    try{const expression=symbolic(cas(n,xc,mode),{},'');exact=expression.toTeX().replace(/\\mathrm\{log\}/g,'\\ln').replace(/\\mathrm\{asin\}/g,'\\arcsin').replace(/\\mathrm\{acos\}/g,'\\arccos').replace(/\\mathrm\{atan\}/g,'\\arctan');symbolic.clear('all');}catch(_){exact='';}
  }
  return {value,decimal,exact,hasX:containsX(n)};
}
const api={parse,containsX,numeric,calculate,InputError};
if(typeof module==='object'&&module.exports)module.exports=api;else root.FunctionCalculatorEngine=api;
})(typeof window==='object'?window:globalThis);
